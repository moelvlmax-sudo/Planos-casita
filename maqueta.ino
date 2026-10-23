#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include <ESP32PWM.h>

// ==========================================
// Configuración del Wi-Fi (Punto de Acceso)
// ==========================================
const char* ssid = "MaquetaDomotica";     // Nombre de la red Wi-Fi
const char* password = "12345678";       // Contraseña del AP

WebServer server(80);

// ==========================================
// Configuración de la Tira LED SK6812 RGBW
// ==========================================
#define LED_PIN        18        // Pin GPIO de la ESP32 conectado al DIN de la tira LED
#define NUM_LEDS       12        // Luces lógicas del plano
#define BRIGHTNESS     220       // Brillo máximo (0-255)

// SK6812 RGBW utiliza el orden de color GRBW y frecuencia de 800kHz
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRBW + NEO_KHZ800);

// Estado de brillo de cada luz (0: Apagado, 255: Encendido)
uint8_t targetBrightness[NUM_LEDS]  = {0};
uint8_t currentBrightness[NUM_LEDS] = {0};

// ==========================================
// Servos (90 grados: 0 cerrado, 90 abierto)
// ==========================================
struct PuertaMap {
  const char* id;
  int pin;
  int closed;
  int open;
  Servo* servo;
};

Servo s1, s2, s3, s4, s5, sg;

PuertaMap PUERTAS[] = {
  // id    pin  cerrado  abierto  servo
  { "p1",   13,    0,       90,     &s1 },
  { "p2",   16,    0,       90,     &s2 },
  { "p3",   27,    0,       90,     &s3 },
  { "p4",   26,    0,       90,     &s4 },
  { "p5",   25,    0,       90,     &s5 },
  { "pg",   33,    0,       90,     &sg }
};
const int PUERTAS_SIZE = sizeof(PUERTAS) / sizeof(PUERTAS[0]);

int currentAngle[PUERTAS_SIZE] = {0};
int targetAngle[PUERTAS_SIZE]  = {0};

int getPuertaIndex(const String& id) {
  for (int i = 0; i < PUERTAS_SIZE; i++)
    if (id == PUERTAS[i].id) return i;
  return -1;
}

// ==========================================
// Mapeo de IDs del HTML al índice del LED en la tira (0 a 11)
// ==========================================
struct LuzMap { const char* id; int index; };
const LuzMap MAPA[] = {
  { "garage_x1", 0 },  // Garage 1
  { "garage_x2", 1 },  // Garage 2
  { "verde_f1",  2 },  // Área verde 1
  { "verde_f2",  3 },  // Área verde 2
  { "bano_x1",   4 },  // Habitación: Baño
  { "dorm_x1",   5 },  // Dormitorio 1
  { "dorm_f2",   6 },  // Dormitorio 2
  { "dorm_f3",   7 },  // Dormitorio 3
  { "cocina_x1", 8 },  // Habitación: Cocina
  { "sala_x1",   9 },  // Habitación: Sala
  { "porche_f1",10 },  // Porche 1
  { "porche_f2",11 }   // Porche 2
};
const int MAPA_SIZE = sizeof(MAPA) / sizeof(MAPA[0]);

int getLedIndex(const String& id) {
  for (int i = 0; i < MAPA_SIZE; i++)
    if (id == MAPA[i].id) return MAPA[i].index;
  return -1;
}

// ==========================================
// Página Web HTML + CSS + JS (plano SVG, 180° visuales)
// ==========================================
const char html_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Maqueta Domótica - SK6812 RGBW & Servos</title>
<style>
:root{--bg:#10141c;--floor:#1a2030;--wall:#8fa3c4;--ink:#c9d4ea;--on:#ffd23f;--off:#4a5675}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font-family:system-ui,sans-serif;padding:12px}
h1{font-size:1.1rem;margin:0 0 8px;text-align:center}
.bar{display:flex;flex-wrap:wrap;gap:8px;align-items:center;justify-content:center;margin-bottom:10px}
button{background:#1f2738;color:var(--ink);border:1px solid #33405e;border-radius:6px;padding:7px 10px;font-size:.85rem;cursor:pointer}
button:hover{border-color:var(--on);color:var(--on)}
#st{font-size:.8rem;opacity:.8;text-align:center;display:block;margin-bottom:6px}
svg{width:100%;max-width:820px;height:auto;display:block;margin:auto}
.room{fill:var(--floor);transition:fill .3s}
.room.lit{fill:#3a3520}
.room.verde{fill:#17261d}.room.verde.lit{fill:#244a30}
.tree .t{fill:#1d3a26;stroke:#4a5675;stroke-width:2;transition:all .3s}
.tree .orn{fill:#4a5675;transition:fill .3s}
.tree .star{fill:#4a5675;transition:fill .3s}
.tree.on .t{fill:#1f8a3b;stroke:#6fe08a}
.tree.on .star{fill:#ffd23f}
.tree.on .orn{animation:bl 1.2s infinite alternate}
.tree.on .orn:nth-of-type(2n){animation-delay:.4s}
.tree.on .orn:nth-of-type(3n){animation-delay:.8s}
@keyframes bl{0%{fill:#ff4d4d}33%{fill:#ffd23f}66%{fill:#4da3ff}100%{fill:#ff7bf0}}
.tree.on{filter:drop-shadow(0 0 9px #4ade80)}
.wall{stroke:var(--wall);stroke-width:5;stroke-linecap:square;fill:none}
.furn{fill:none;stroke:#5d6d92;stroke-width:1.5}
.lbl{fill:#6d7fa6;font-size:13px;letter-spacing:1px;text-anchor:middle;pointer-events:none}
.dim{fill:#6d7fa6;font-size:11px}
.light{cursor:pointer;transform-box:fill-box;transform-origin:center;transition:transform .2s}
.light:hover{transform:scale(1.15)}
.light .sh{fill:#1a2030;stroke:var(--off);stroke-width:2.5;transition:all .25s}
.light .cr{stroke:var(--off);stroke-width:2.5;transition:stroke .25s}
.light.on .sh{fill:#4d4210;stroke:var(--on)}
.light.on .cr{stroke:var(--on)}
.light.on{filter:drop-shadow(0 0 7px var(--on))}
.door{cursor:pointer}
.leaf{stroke:#e8b86d;stroke-width:5;stroke-linecap:round}
.arc{fill:none;stroke:#6d7fa6;stroke-width:1.5;stroke-dasharray:5 4}
.dl{transition:transform .7s cubic-bezier(.4,0,.2,1)}
.win{stroke:#3ddc84;stroke-width:6;fill:none}
.winc{stroke:#10141c;stroke-width:1.5;fill:none}
.zona{stroke:#6d7fa6;stroke-width:3;stroke-dasharray:10 7;fill:none}
.open{stroke:#33405e;stroke-width:2;stroke-dasharray:4 4;fill:none}
.gdoor{cursor:pointer}
.slat{stroke:#e5383b;stroke-width:8}
.gl{transition:transform .9s cubic-bezier(.5,0,.2,1)}
.garrow{fill:none;stroke:#e5383b;stroke-width:2;stroke-dasharray:5 4;opacity:.85}
.gdoor:hover .slat{stroke:#ff5a5f}
.leyenda{max-width:820px;margin:10px auto 0;font-size:.8rem;color:#8fa3c4;display:flex;flex-wrap:wrap;gap:6px 18px;justify-content:center}
.leyenda span{display:inline-flex;align-items:center;gap:6px}
.leyenda i{display:inline-block;width:16px;height:6px;border-radius:2px}
</style>
</head>
<body>
<h1>🏠 Maqueta Domótica</h1>
<div class="bar">
  <button id="all1">Encender todo</button>
  <button id="all0">Apagar todo</button>
</div>
<span id="st">Listo</span>
<svg id="p" viewBox="0 0 930 770" xmlns="http://www.w3.org/2000/svg"></svg>
<div class="leyenda">
  <span><i style="background:#3ddc84"></i>Ventana = verde</span>
  <span><i style="background:#e5383b"></i>Puerta del garaje = rojo</span>
  <span><i style="background:#e8b86d"></i>Puerta común = amarillo</span>
</div>

<script>
const NS='http://www.w3.org/2000/svg',svg=document.getElementById('p');
const $=(t,a={},par=svg,txt)=>{const e=document.createElementNS(NS,t);for(const k in a)e.setAttribute(k,a[k]);if(txt)e.textContent=txt;par.appendChild(e);return e};

// Habitaciones
const rooms={
 garage:[20,20,550,230],verde:[570,20,120,230],
 bano:[20,250,330,240],cocina:[350,250,340,240],
 dorm:[20,490,330,250],sala:[350,490,340,250],porche:[690,370,90,370]
};
const roomEls={};
for(const k in rooms){const [x,y,w,h]=rooms[k];roomEls[k]=$('rect',{x,y,width:w,height:h,class:'room'})}
roomEls.verde.classList.add('verde');
const df=$('defs');const pt=$('pattern',{id:'g',width:10,height:10,patternUnits:'userSpaceOnUse'},df);
$('path',{d:'M0 10L10 0',stroke:'#2d4a37','stroke-width':1},pt);
$('rect',{x:570,y:20,width:120,height:230,fill:'url(#g)'});

// Muros
const W=(d)=>$('path',{d,class:'wall'});
W('M780 740H20V20H690');
$('path',{d:'M690 20V250M780 370V740',class:'zona'});
W('M570 20V90M570 190V250');
W('M20 250H358M418 250H690');
W('M350 250V330M350 390V490');
W('M350 490V585M350 645V740');
W('M20 490H70M130 490H350');
W('M350 490H440M600 490H690');
$('path',{d:'M440 490H600',class:'open'});
W('M690 250V410M690 470V677M690 737V740');
W('M690 370H780');
$('path',{d:'M690 410V470',class:'win'});$('path',{d:'M690 410V470',class:'winc'});

// Mobiliario
const R=(x,y,w,h,rx=0)=>$('rect',{x,y,width:w,height:h,rx,class:'furn'});
const C=(x,y,r)=>$('circle',{cx:x,cy:y,r,class:'furn'});
R(40,265,38,54,12);R(40,345,34,44,10);
R(40,570,120,130,4);R(40,542,24,24);R(40,704,24,24);R(46,580,40,32,6);R(46,658,40,32,6);
R(428,253,259,34);R(653,253,34,150);R(430,375,140,28,3);C(450,432,11);C(500,432,11);C(550,432,11);
R(420,670,200,50,12);R(445,670,150,26,6);$('ellipse',{cx:520,cy:610,rx:50,ry:30,class:'furn'});
[['garage','GARAGE',295,45],['verde','ÁREA VERDE',630,240],['bano','BAÑO',185,278],['cocina','COCINA / COMEDOR',520,468],
 ['dorm','DORMITORIO',270,727],['sala','SALA DE ESTAR',520,725],['porche','PORCHE',735,725]].forEach(([k,t,x,y])=>$('text',{x,y,class:'lbl'},svg,t));
$('text',{x:870,y:442,class:'lbl'},svg,'EXTERIOR');
$('text',{x:340,y:765,class:'dim'},svg,'20 cm');$('text',{x:728,y:765,class:'dim'},svg,'5 cm');
$('text',{x:2,y:140,class:'dim'},svg,'10,4');$('text',{x:575,y:14,class:'dim'},svg,'3,14 cm');

const rad=d=>d*Math.PI/180;
const st=document.getElementById('st');
const ESP_IP_OVERRIDE = '';
const BASE = ESP_IP_OVERRIDE
  ? `http://${ESP_IP_OVERRIDE}`
  : (location.protocol.startsWith('http') ? '' : 'http://192.168.4.1');

async function enviarPuerta(id, open){
  try{
    const r = await fetch(`${BASE}/puerta?id=${encodeURIComponent(id)}&estado=${open?'open':'close'}`);
    st.textContent = r.ok ? `✔ Puerta ${id} → ${open?'Abierta':'Cerrada'}` : `⚠ Error ${r.status}`;
  }catch(e){
    st.textContent = '⚠ No se pudo conectar al ESP32';
  }
}

// Puerta: 's' es el angulo visual del SVG
function door(id,hx,hy,a,s,L=60){
 const g=$('g',{class:'door',id});
 const ce=[hx+L*Math.cos(rad(a)),hy+L*Math.sin(rad(a))],oe=[hx+L*Math.cos(rad(a+s)),hy+L*Math.sin(rad(a+s))];
 $('path',{class:'arc',d:`M${ce[0]} ${ce[1]}A${L} ${L} 0 0 ${s>0?1:0} ${oe[0]} ${oe[1]}`},g);
 const dl=$('g',{class:'dl'},g);dl.style.transformOrigin=`${hx}px ${hy}px`;
 $('line',{class:'leaf',x1:hx,y1:hy,x2:ce[0],y2:ce[1]},dl);
 $('circle',{cx:hx,cy:hy,r:4,fill:'#e8b86d'},g);
 let open=false;
 g.addEventListener('click',()=>{
   open=!open;
   dl.style.transform=open?`rotate(${s}deg)`:'none';
   enviarPuerta(id, open);
 });
}
door('p1',70,490,0,-90);
door('p2',350,330,90,180);
door('p3',350,585,90,180);
door('p4',690,737,270,-90);
door('p5',358,250,0,-180);

// Puerta del garage
(function(){
 const g=$('g',{class:'gdoor',id:'pg'});
 $('title',{},g,'Puerta del garage');
 $('rect',{x:562,y:84,width:16,height:112,fill:'transparent'},g);
 $('path',{class:'garrow',d:'M556 185V99M550 107L556 98L562 107'},g);
 const gl=$('g',{class:'gl'},g);gl.style.transformOrigin='570px 90px';
 for(let i=0;i<5;i++)$('line',{class:'slat',x1:570,y1:90+i*20,x2:570,y2:106+i*20},gl);
 $('rect',{x:563,y:82,width:14,height:8,rx:2,fill:'#9d1f26'},g);
 let o=false;
 g.addEventListener('click',()=>{
   o=!o;
   gl.style.transform=o?'scaleY(.1)':'none';
   enviarPuerta('pg', o);
 });
})();

// Luces del plano
const luces=[
 ['garage_x1','x',240,135,'garage'],  // Garage 1
 ['garage_x2','x',440,135,'garage'],  // Garage 2
 ['verde_f1','f',590,55,'verde'],     // Área verde 1
 ['verde_f2','f',590,215,'verde'],    // Área verde 2
 ['bano_x1','x',200,370,'bano'],      // Habitación: Baño
 ['dorm_x1','x',240,620,'dorm'],      // Dormitorio 1
 ['dorm_f2','f',52,554,'dorm'],       // Dormitorio 2
 ['dorm_f3','f',52,716,'dorm'],       // Dormitorio 3
 ['cocina_x1','x',470,335,'cocina'],  // Habitación: Cocina
 ['sala_x1','x',520,535,'sala'],      // Habitación: Sala
 ['porche_f1','f',706,640,'porche'],  // Porche 1
 ['porche_f2','f',705,395,'porche'],  // Porche 2
 ['verde_arbol','t',870,360,'ext']    // Árbol decorativo (sin LED físico)
];

const estado={};

async function enviar(id,on){
  try{
    const r = await fetch(`${BASE}/luz?id=${encodeURIComponent(id)}&estado=${on?'on':'off'}`);
    st.textContent = r.ok ? `✔ ${id} → ${on?'ON':'OFF'}` : `⚠ Error ${r.status}`;
  }catch(e){
    st.textContent = '⚠ No se pudo conectar al ESP32';
  }
}

function refrescarCuarto(r){
 if(!roomEls[r])return;
 const n=luces.filter(l=>l[4]===r&&estado[l[0]]).length;
 roomEls[r].classList.toggle('lit',n>0);
}

function set(id,on,send=true){
 const l=luces.find(l=>l[0]===id);
 if(!l)return;
 estado[id]=on;
 const el=document.getElementById(id);
 if(el) el.classList.toggle('on',on);
 refrescarCuarto(l[4]);
 if(send) enviar(id,on);
}

luces.forEach(([id,t,x,y,r])=>{
 const g=$('g',{class:'light',id});estado[id]=false;
 if(t==='t'){
  g.setAttribute('class','light tree');
  const o=x-710,oy=y-140,sh=q=>q.split(' ').map(v=>{const[a,b]=v.split(',');return(+a+o)+','+(+b+oy)}).join(' ');
  $('rect',{x:703+o,y:176+oy,width:14,height:22,fill:'#6b4a2b'},g);
  ['710,92 676,132 744,132','710,116 670,164 750,164','710,142 664,184 756,184'].forEach(p=>$('polygon',{class:'t',points:sh(p)},g));
  $('polygon',{class:'star',points:sh('710,64 714,76 727,76 717,84 721,97 710,89 699,97 703,84 693,76 706,76')},g);
  [[700,120],[722,140],[690,158],[728,168],[708,176],[696,176]].forEach(([a,b])=>$('circle',{class:'orn',cx:a+o,cy:b+oy,r:4},g));
 }else if(t==='x'){
  $('rect',{class:'sh',x:x-16,y:y-16,width:32,height:32,rx:2},g);
  $('path',{class:'cr',d:`M${x-12} ${y-12}L${x+12} ${y+12}M${x+12} ${y-12}L${x-12} ${y+12}`},g);
 }else{
  $('circle',{class:'sh',cx:x,cy:y,r:12},g);
  $('path',{class:'cr',d:`M${x-8} ${y-8}L${x+8} ${y+8}M${x+8} ${y-8}L${x-8} ${y+8}`},g);
 }
 $('title',{},g,id);
 g.addEventListener('click',()=>set(id,!estado[id]));
});

async function todas(on){
  luces.forEach(l=>set(l[0],on,false));
  try{
    const r = await fetch(`${BASE}/todas?estado=${on?'on':'off'}`);
    st.textContent = r.ok ? `✔ Todas → ${on?'ON':'OFF'}` : `⚠ Error ${r.status}`;
  }catch(e){
    st.textContent = '⚠ No se pudo conectar al ESP32';
  }
}
document.getElementById('all1').onclick=()=>todas(true);
document.getElementById('all0').onclick=()=>todas(false);
</script>
</body>
</html>
)rawliteral";

// ==========================================
// Handlers del Servidor HTTP
// ==========================================
void addCORS(){
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
}

// Página raíz
void handleRoot() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  // send_P sirve la página directo desde flash, sin copiarla a la RAM
  server.send_P(200, "text/html; charset=utf-8", html_page);
}

// Encender/apagar una luz
void handleLuz() {
  addCORS();
  if (!server.hasArg("id") || !server.hasArg("estado")) {
    server.send(400, "text/plain", "Parámetros incompletos");
    return;
  }
  String id   = server.arg("id");
  bool   on   = server.arg("estado") == "on";
  int    index = getLedIndex(id);

  if (index < 0) {
    server.send(200, "text/plain", "OK (sin LED asociado)");
    return;
  }
  if (index >= NUM_LEDS) {
    Serial.printf("Luz [%s] sin LED fisico (idx %d >= %d)\n", id.c_str(), index, NUM_LEDS);
    server.send(200, "text/plain", "OK (sin LED fisico)");
    return;
  }
  targetBrightness[index] = on ? BRIGHTNESS : 0;
  Serial.printf("Luz [%s] -> LED #%d: %s\n", id.c_str(), index, on ? "ON" : "OFF");
  server.send(200, "text/plain", "OK");
}

// Encender/apagar todas las luces
void handleTodas() {
  addCORS();
  bool on = server.hasArg("estado") && server.arg("estado") == "on";
  for (int i = 0; i < NUM_LEDS; i++) targetBrightness[i] = on ? BRIGHTNESS : 0;
  Serial.printf("TODAS -> %s (%d LEDs)\n", on ? "ON" : "OFF", NUM_LEDS);
  server.send(200, "text/plain", "OK");
}

// Abrir/cerrar una puerta
void handlePuerta() {
  addCORS();
  if (!server.hasArg("id") || !server.hasArg("estado")) {
    server.send(400, "text/plain", "Parámetros incompletos");
    return;
  }
  String id   = server.arg("id");
  bool   open = server.arg("estado") == "open";
  int    idx  = getPuertaIndex(id);
  if (idx < 0) {
    server.send(400, "text/plain", "Puerta desconocida");
    return;
  }

  int ang = open ? PUERTAS[idx].open : PUERTAS[idx].closed;
  ang = constrain(ang, 0, 90);
  targetAngle[idx] = ang;

  Serial.printf("Puerta [%s] -> %s (servo %d deg)\n",
                id.c_str(), open ? "ABIERTA" : "CERRADA", ang);
  server.send(200, "text/plain", "OK");
}

// Respuesta preflight CORS
void handleOptions(){
  addCORS();
  server.send(204);
}

// Favicon vacío
void handleFavicon(){
  server.send(204);
}

// ==========================================
// Setup y Loop Principal
// ==========================================
void setup() {
  Serial.begin(115200);

  // Inicializar tira SK6812 RGBW
  strip.begin();
  strip.show(); // Apagar todos al iniciar

  // Reservar timers PWM para los servos
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // Inicializar servos
  for (int i = 0; i < PUERTAS_SIZE; i++) {
    PUERTAS[i].servo->setPeriodHertz(50);
    PUERTAS[i].servo->attach(PUERTAS[i].pin, 500, 2400);
    PUERTAS[i].servo->write(PUERTAS[i].closed);
    currentAngle[i] = PUERTAS[i].closed;
    targetAngle[i]  = PUERTAS[i].closed;
    delay(50);
  }

  // Configuración del Punto de Acceso Wi-Fi
  Serial.println("\nIniciando Access Point...");
  if (!WiFi.softAP(ssid, password)) {
    Serial.println("ERROR: no se pudo iniciar el Access Point");
  }
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("IP del servidor: ");
  Serial.println(myIP); // http://192.168.4.1

  // Rutas
  server.on("/",            HTTP_GET,     handleRoot);
  server.on("/luz",         HTTP_GET,     handleLuz);
  server.on("/todas",       HTTP_GET,     handleTodas);
  server.on("/puerta",      HTTP_GET,     handlePuerta);
  server.on("/luz",         HTTP_OPTIONS, handleOptions);
  server.on("/todas",       HTTP_OPTIONS, handleOptions);
  server.on("/puerta",      HTTP_OPTIONS, handleOptions);
  server.on("/favicon.ico", HTTP_GET,     handleFavicon);
  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

void loop() {
  server.handleClient();

  // Transición suave (fade in / fade out) para SK6812 RGBW
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 8) { // Velocidad del fade (cada 8ms)
    lastUpdate = millis();
    bool changed = false;

    for (int i = 0; i < NUM_LEDS; i++) {
      if (currentBrightness[i] < targetBrightness[i]) {
        currentBrightness[i] = min((int)currentBrightness[i] + 4, (int)targetBrightness[i]);
        changed = true;
      } else if (currentBrightness[i] > targetBrightness[i]) {
        currentBrightness[i] = max((int)currentBrightness[i] - 4, (int)targetBrightness[i]);
        changed = true;
      }

      // Parámetros de color SK6812 RGBW: (R, G, B, W)
      // Usamos el canal W (Blanco puro) al máximo con un ligero toque de R y G para una iluminación de hogar cálida.
      uint8_t w = (230 * currentBrightness[i]) / 255; // Canal Blanco principal
      uint8_t r = (30  * currentBrightness[i]) / 255; // Un toque cálido de rojo
      uint8_t g = (15  * currentBrightness[i]) / 255; // Un toque cálido de verde
      uint8_t b = 0;                                  // Canal Azul apagado para evitar luz fría

      // Sintaxis de 4 canales para Adafruit NeoPixel en SK6812 RGBW
      strip.setPixelColor(i, strip.Color(r, g, b, w));
    }

    if (changed) {
      strip.show();
    }
  }

  // Rampa servos (cada 20 ms, paso 3 grados)
  static unsigned long lastServo = 0;
  if (millis() - lastServo > 20) {
    lastServo = millis();
    for (int i = 0; i < PUERTAS_SIZE; i++) {
      if (currentAngle[i] == targetAngle[i]) continue;
      int step = (currentAngle[i] < targetAngle[i]) ? 3 : -3;
      currentAngle[i] += step;
      if ((step > 0 && currentAngle[i] > targetAngle[i]) ||
          (step < 0 && currentAngle[i] < targetAngle[i])) {
        currentAngle[i] = targetAngle[i];
      }
      PUERTAS[i].servo->write(currentAngle[i]);
    }
  }
}
