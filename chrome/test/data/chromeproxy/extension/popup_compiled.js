// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/*
 jQuery JavaScript Library v1.9.0
 http://jquery.com/

 Includes Sizzle.js
 http://sizzlejs.com/

 Copyright 2005, 2012 jQuery Foundation, Inc. and other contributors
 Released under the MIT license
 http://jquery.org/license

 Date: 2013-1-14
*/
(function(v,p){function m(a){var f=a.length,k=c.type(a);
return c.isWindow(a)?!1:1===a.nodeType&&f?!0:"array"===k||"function"
!==k&&(0===f||"number"==typeof f&&0<f&&f-1 in a)}function d(a){
var f=kb[a]={};return c.each(a.match(Z)||[],function(a,c){f[c]=!0}),f}
function n(a,f,k,b){if(c.acceptData(a)){var g,l,u=c.expando,
x="string"==typeof f,e=a.nodeType,d=e?c.cache:a,q=e?a[u]:a[u]&&u;
if(q&&d[q]&&(b||d[q].data)||!x||k!==p)return q||(e?a[u]=q=I.pop()
||c.guid++:q=u),d[q]||(d[q]={},e||(d[q].toJSON=c.noop)),
("object"==typeof f||"function"==typeof f)&&(b?d[q]=c.extend(d[q],f)
:d[q].data=c.extend(d[q].data,f)),g=d[q],b||(g.data||(g.data={}),g=g.data)
,k!==p&&(g[c.camelCase(f)]=k),x?(l=g[f],null==l&&(l=g[c.camelCase(f)]))
:l=g,l}}function h(a,f,k){if(c.acceptData(a)){var b,g,l,u=a.nodeType,x=u?
c.cache:a,e=u?a[c.expando]:c.expando;if(x[e]){if(f&&(b=k?x[e]:x[e].data))
{c.isArray(f)?f=f.concat(c.map(f,c.camelCase)):f in b?f=[f]:(
f=c.camelCase(f),f=f in b?[f]:f.split(" "));g=0;for(l=f.length;l>g;g++)
delete b[f[g]];
if(!(k?B:c.isEmptyObject)(b))return}(k||(delete x[e].data,B(x[e])))&&(u?
c.cleanData([a],!0):c.support.deleteExpando||x!=x.window?delete x[e]:x[e]=null
)
}}}function r(a,f,k){if(k===p&&1===a.nodeType){var b="data-"+f.replace(
Jb,"-$1").toLowerCase();if(k=a.getAttribute(b),"string"==typeof k){try{
k="true"===k?!0:"false"===k?!1:"null"===k?null:+k+""===k?+k:Kb.test(k)?
c.parseJSON(k):k}catch(g){}c.data(a,f,k)}else k=p}return k}function B(
a){for(var f in a)if(("data"!==f||!c.isEmptyObject(a[f]))&&"toJSON"!==
f)return!1;return!0}function D(){return!0}function M(){return!1}
function E(a,f){do a=a[f];while(a&&1!==a.nodeType);return a}function A(
a,f,k){if(f=f||0,c.isFunction(f))return c.grep(a,function(a,c){var b=!!
f.call(a,c,a);return b===k});if(f.nodeType)return c.grep(a,function(a){
return a===f===k});if("string"==typeof f){var b=c.grep(a,function(a){
return 1===a.nodeType});if(Lb.test(f))return c.filter(f,b,!k);f=c.filter(
f,b)}return c.grep(a,function(a){return 0<=c.inArray(a,f)===k})}function V(
a){var f=
lb.split("|");a=a.createDocumentFragment();if(a.createElement)for(;f.length;)
f.pop();return a}function ra(a,f){return a.getElementsByTagName(f)[0]||
a.appendChild(a.ownerDocument.createElement(f))}function Q(a){
var f=a.getAttributeNode("type");return a.type=(f&&f.specified)+"/"+a.type,a}
function ea(a){var f=Mb.exec(a.type);return f?a.type=f[1]:
a.removeAttribute("type"),a}function R(a,f){for(var k,b=0;null!=(k=a[b]);b++)
c._data(k,"globalEval",!f||c._data(f[b],"globalEval"))}function sa(a,f){if
(1===
f.nodeType&&c.hasData(a)){var k,b,g;b=c._data(a);var l=c._data(f,b),u=b.events
;
if(u)for(k in delete l.handle,l.events={},u)for(b=0,g=u[k].length;g>b;b++)
c.event.add(f,k,u[k][b]);l.data&&(l.data=c.extend({},l.data))}}function F(a,f)
{var k,b,g=0,l=a.getElementsByTagName!==p?a.getElementsByTagName(f||"*"):
a.querySelectorAll!==p?a.querySelectorAll(f||"*"):p;if(!l)for(l=[],
k=a.childNodes||a;null!=(b=k[g]);g++)!f||c.nodeName(b,f)?l.push(b):
c.merge(l,F(b,f));return f===p||f&&c.nodeName(a,f)?c.merge([a],
l):l}function Da(a){Ua.test(a.type)&&(a.defaultChecked=a.checked)}
function Ea(a,f){if(f in a)return f;for(
var c=f.charAt(0).toUpperCase()+f.slice(1),b=f,g=mb.length;g--;)
if(f=mb[g]+c,f in a)return f;return b}function S(a,f){return a=f||a,
"none"===c.css(a,"display")||!c.contains(a.ownerDocument,a)}function X(a,f){
for(var k,b=[],g=0,l=a.length;l>g;g++)k=a[g],k.style&&(b[g]=c._data(k,
"olddisplay"),f?(b[g]||"none"!==k.style.display||(k.style.display=""),""===
k.style.display&&S(k)&&(b[g]=c._data(k,"olddisplay",
Fa(k.nodeName)))):b[g]||S(k)||c._data(k,"olddisplay",c.css(k,"display")));
for(g=0;l>g;g++)k=a[g],k.style&&(f&&"none"!==k.style.display&&""!==
k.style.display||(k.style.display=f?b[g]||"":"none"));return a}function va(
a,f,c){return(a=Nb.exec(f))?Math.max(0,a[1]-(c||0))+(a[2]||"px"):f}
function Ga(a,f,k,b,g){f=k===(b?"border":"content")?4:"width"===f?1:0;
for(var l=0;4>f;f+=2)"margin"===k&&(l+=c.css(a,k+fa[f],!0,g)),b?(
"content"===k&&(l-=c.css(a,"padding"+fa[f],!0,g)),"margin"!==k&&(l-=c.css(a,
"border"+
fa[f]+"Width",!0,g))):(l+=c.css(a,"padding"+fa[f],!0,g),"padding"!==k&&(
l+=c.css(a,"border"+fa[f]+"Width",!0,g)));return l}function Ha(a,f,k){
var b=!0,g="width"===f?a.offsetWidth:a.offsetHeight,l=ga(a),
u=c.support.boxSizing&&"border-box"===c.css(a,"boxSizing",!1,l);if(0>=g||
null==g){if(g=Y(a,f,l),(0>g||null==g)&&(g=a.style[f]),Ia.test(g))return g;
b=u&&(c.support.boxSizingReliable||g===a.style[f]);g=parseFloat(g)||0}
return g+Ga(a,f,k||(u?"border":"content"),b,l)+"px"}function Fa(a){
var f=y,k=nb[a];
return k||(k=Ja(a,f),"none"!==k&&k||(wa=(wa||c(
"<iframe frameborder='0' width='0' height='0'/>").css(
"cssText","display:block !important")).appendTo(f.documentElement),
f=(wa[0].contentWindow||wa[0].contentDocument).document,f.write(
"<!doctype html><html><body>"),f.close(),k=Ja(a,f),wa.detach()),nb[a]=k),
k}function Ja(a,f){var k=c(f.createElement(a)).appendTo(f.body),
b=c.css(k[0],"display");return k.remove(),b}function xa(a,f,k,b){
var g;if(c.isArray(f))c.each(f,function(f,c){k||Ob.test(a)?b(a,c):xa(a+
"["+("object"==typeof c?f:"")+"]",c,k,b)});else if(k||"object"!==c.type(
f))b(a,f);else for(g in f)xa(a+"["+g+"]",f[g],k,b)}function Ka(a){
return function(f,k){"string"!=typeof f&&(k=f,f="*");var b,g=0,l=
f.toLowerCase().match(Z)||[];if(c.isFunction(k))for(;b=l[g++];)"+"===b[0]?
(b=b.slice(1)||"*",(a[b]=a[b]||[]).unshift(k)):(a[b]=a[b]||[]).push(k)
}}function La(a,f,k,b){function g(x){var e;return l[x]=!0,c.each(a[x]||[],
function(a,c){var x=c(f,k,b);return"string"!=typeof x||u||l[x]?u?!(e=x):p:(
f.dataTypes.unshift(x),
g(x),!1)}),e}var l={},u=a===Wa;return g(f.dataTypes[0])||!l["*"]&&g("*")}
function P(a,f){var k,b,g=c.ajaxSettings.flatOptions||{};for(k in f)
f[k]!==p&&((g[k]?a:b||(b={}))[k]=f[k]);return b&&c.extend(!0,a,b),a}
function ka(){try{return new v.XMLHttpRequest}catch(a){}}function ya()
{return setTimeout(function(){la=p}),la=c.now()}function b(a,f){c.each(
f,function(f,c){for(var b=(za[f]||[]).concat(za["*"]),l=0,u=b.length;
u>l&&!b[l].call(a,f,c);l++);})}function e(a,f,k){var t,g=0,l=Ma.length,
u=c.Deferred().always(function(){delete x.elem}),
x=function(){if(t)return!1;for(var f=la||ya(),f=Math.max(
0,e.startTime+e.duration-f),c=f/e.duration||0,c=1-c,k=0,b=e.tweens.length;
b>k;k++)e.tweens[k].run(c);return u.notifyWith(a,[e,c,f]),1>c&&b?f:(
u.resolveWith(a,[e]),!1)},e=u.promise({elem:a,props:c.extend({},f),
opts:c.extend(!0,{specialEasing:{}},k),originalProperties:f,
originalOptions:k,startTime:la||ya(),duration:k.duration,tweens:[],
createTween:function(f,k){var b=c.Tween(a,e.opts,f,k,e.opts.specialEasing[f]
||e.opts.easing);return e.tweens.push(b),
b},stop:function(f){var c=0,k=f?e.tweens.length:0;if(t)return this;for(
t=!0;k>c;c++)e.tweens[c].run(1);return f?u.resolveWith(a,[e,f]):
u.rejectWith(a,[e,f]),this}});k=e.props;for(q(k,e.opts.specialEasing);
l>g;g++)if(f=Ma[g].call(e,a,k,e.opts))return f;return b(e,k),c.isFunction(
e.opts.start)&&e.opts.start.call(a,e),c.fx.timer(c.extend(x,{elem:a,
anim:e,queue:e.opts.queue})),e.progress(e.opts.progress).done(
e.opts.done,e.opts.complete).fail(e.opts.fail).always(e.opts.always)}
function q(a,f){var k,b,
g,l,u;for(k in a)if(b=c.camelCase(k),g=f[b],l=a[k],c.isArray(l)&&(
g=l[1],l=a[k]=l[0]),k!==b&&(a[b]=l,delete a[k]),u=c.cssHooks[b],
u&&"expand"in u)for(k in l=u.expand(l),delete a[b],l)k in a||(a[k]=l[k],
f[k]=g);else f[b]=g}function w(a,f,k){var b,g,l,u,x,e,d=this,q=a.style,h={},
m=[],n=a.nodeType&&S(a);k.queue||(x=c._queueHooks(a,"fx"),null==x.unqueued&&(x
.unqueued=0,e=x.empty.fire,x.empty.fire=function(){x.unqueued||e()}),
x.unqueued++,d.always(function(){d.always(function(){x.unqueued--;c.queue(a,
"fx").length||x.empty.fire()})}));1===a.nodeType&&("height"in f||"width"in f)
&&(k.overflow=[q.overflow,q.overflowX,q.overflowY],"inline"===c.css(a
,"display")&&"none"===c.css(a,"float")&&(c.support.inlineBlockNeedsLayout&
&"inline"!==Fa(a.nodeName)?q.zoom=1:q.display="inline-block"));k.overflow&&(q
.overflow="hidden",c.support.shrinkWrapBlocks||d.done(function(){q.overflow=k
.overflow[0];q.overflowX=k.overflow[1];q.overflowY=k.overflow[2]}));for(b in f
)(l=f[b],Qb.exec(l))&&(delete f[b],g=g||"toggle"===
l,l!==(n?"hide":"show"))&&m.push(b);if(f=m.length)for(l=c._data(a,"fxshow")||c
._data(a,"fxshow",{}),("hidden"in l)&&(n=l.hidden),g&&(l.hidden=!n),n?c(a)
.show():d.done(function(){c(a).hide()}),d.done(function(){var f;c._removeData
(a,"fxshow");for(f in h)c.style(a,f,h[f])}),b=0;f>b;b++)g=m[b],u=d.createTween
(g,n?l[g]:0),h[g]=l[g]||c.style(a,g),g in l||(l[g]=u.start,n&&(u.end=u.start,u
.start="width"===g||"height"===g?1:0))}function z(a,f,c,b,g){return new z
.prototype.init(a,f,c,b,g)}function J(a,f){var c,
b={height:a},g=0;for(f=f?1:0;4>g;g+=2-f)c=fa[g],b["margin"+c]=b["padding"+c]=a
;return f&&(b.opacity=b.width=a),b}function N(a){return c.isWindow(a)?a:9===a
.nodeType?a.defaultView||a.parentWindow:!1}var O,G,y=v.document,Na=v.location
,Aa=v.jQuery,Rb=v.$,ha={},I=[],Oa=I.concat,Xa=I.push,aa=I.slice,ob=I.indexOf
,Sb=ha.toString,Ya=ha.hasOwnProperty,Za="1.9.0".trim,c=function(a,f){return
 new c.fn.init(a,f,O)},Pa=/[+-]?(?:\d*\.|)\d+(?:[eE][+-]?\d+|)/.source
,Z=/\S+/g,Tb=/^[\s\uFEFF\xA0]+|[\s\uFEFF\xA0]+$/g,
Ub=/^(?:(<[\w\W]+>)[^>]*|#([\w-]*))$/,pb=/^<(\w+)\s*\/?>(?:<\/\1>|)$/,Vb=/^[\]
,:{}\s]*$/,Wb=/(?:^|:|,)(?:\s*\[)+/g,Xb=/\\(?:["\\\/bfnrt]|u[\da-fA-F]{4})/g
,Yb=/"[^"\\\r\n]*"|true|false|null|-?(?:\d+\.|)\d+(?:[eE][+-]?\d+|)/g
,Zb=/^-ms-/,$b=/-([\da-z])/gi,ac=function(a,f){return f.toUpperCase()}
,Qa=function(){y.addEventListener?(y.removeEventListener("DOMContentLoaded",Qa
,!1),c.ready()):"complete"===y.readyState&&(y.detachEvent("onreadystatechange"
,Qa),c.ready())};c.fn=c.prototype={jquery:"1.9.0",constructor:c,
init:function(a,f,k){var b,g;if(!a)return this;if("string"==typeof a){if
(b="<"===a.charAt(0)&&">"===a.charAt(a.length-1)&&3<=a.length?[null,a,null]:Ub
.exec(a),!b||!b[1]&&f)return!f||f.jquery?(f||k).find(a):this.constructor(f)
.find(a);if(b[1]){if(f=f instanceof c?f[0]:f,c.merge(this,c.parseHTML(b[1],f&
&f.nodeType?f.ownerDocument||f:y,!0)),pb.test(b[1])&&c.isPlainObject(f))for(b
 in f)c.isFunction(this[b])?this[b](f[b]):this.attr(b,f[b]);return this}if(g=y
.getElementById(b[2]),g&&g.parentNode){if(g.id!==
b[2])return k.find(a);this.length=1;this[0]=g}return this.context=y,this
.selector=a,this}return a.nodeType?(this.context=this[0]=a,this.length=1,this
):c.isFunction(a)?k.ready(a):(a.selector!==p&&(this.selector=a.selector,this
.context=a.context),c.makeArray(a,this))},selector:"",length:0,size:function()
{return this.length},toArray:function(){return aa.call(this)},get:function(a)
{return null==a?this.toArray():0>a?this[this.length+a]:this[a]}
,pushStack:function(a){a=c.merge(this.constructor(),a);return a.prevObject=
this,a.context=this.context,a},each:function(a,f){return c.each(this,a,f)}
,ready:function(a){return c.ready.promise().done(a),this},slice:function()
{return this.pushStack(aa.apply(this,arguments))},first:function(){return this
.eq(0)},last:function(){return this.eq(-1)},eq:function(a){var f=this.length
;a=+a+(0>a?f:0);return this.pushStack(0<=a&&f>a?[this[a]]:[])},map:function(a)
{return this.pushStack(c.map(this,function(f,c){return a.call(f,c,f)}))}
,end:function(){return this.prevObject||this.constructor(null)},
push:Xa,sort:[].sort,splice:[].splice};c.fn.init.prototype=c.fn;c.extend=c.fn
.extend=function(){var a,f,k,b,g,l,u=arguments[0]||{},x=1,e=arguments.length
,d=!1;"boolean"==typeof u&&(d=u,u=arguments[1]||{},x=2);"object"==typeof u||c
.isFunction(u)||(u={});for(e===x&&(u=this,--x);e>x;x++)if(null!=(a=arguments[x
]))for(f in a)k=u[f],b=a[f],u!==b&&(d&&b&&(c.isPlainObject(b)||(g=c.isArray(b)
))?(g?(g=!1,l=k&&c.isArray(k)?k:[]):l=k&&c.isPlainObject(k)?k:{},u[f]=c.extend
(d,l,b)):b!==p&&(u[f]=b));return u};c.extend({noConflict:function(a){return v
.$===
c&&(v.$=Rb),a&&v.jQuery===c&&(v.jQuery=Aa),c},isReady:!1,readyWait:1
,holdReady:function(a){a?c.readyWait++:c.ready(!0)},ready:function(a){if
(!0===a?!--c.readyWait:!c.isReady){if(!y.body)return setTimeout(c.ready);c
.isReady=!0;!0!==a&&0<--c.readyWait||(G.resolveWith(y,[c]),c.fn.trigger&&c(y)
.trigger("ready").off("ready"))}},isFunction:function(a){return"function"===c
.type(a)},isArray:Array.isArray||function(a){return"array"===c.type(a)}
,isWindow:function(a){return null!=a&&a==a.window},isNumeric:function(a)
{return!isNaN(parseFloat(a))&&
isFinite(a)},type:function(a){return null==a?a+"":"object"==typeof a|
|"function"==typeof a?ha[Sb.call(a)]||"object":typeof a}
,isPlainObject:function(a){if(!a||"object"!==c.type(a)||a.nodeType||c.isWindow
(a))return!1;try{if(a.constructor&&!Ya.call(a,"constructor")&&!Ya.call(a
.constructor.prototype,"isPrototypeOf"))return!1}catch(f){return!1}for(var b
 in a);return b===p||Ya.call(a,b)},isEmptyObject:function(a){for(var f in a
)return!1;return!0},error:function(a){throw Error(a);},parseHTML:function(a,
f,b){if(!a||"string"!=typeof a)return null;"boolean"==typeof f&&(b=f,f=!1);f=f
||y;var t=pb.exec(a);b=!b&&[];return t?[f.createElement(t[1])]:(t=c
.buildFragment([a],f,b),b&&c(b).remove(),c.merge([],t.childNodes))}
,parseJSON:function(a){return v.JSON&&v.JSON.parse?v.JSON.parse(a
):null===a?a:"string"==typeof a&&(a=c.trim(a),a&&Vb.test(a.replace(Xb,"@")
.replace(Yb,"]").replace(Wb,"")))?Function("return "+a)():(c.error("Invalid
 JSON: "+a),p)},parseXML:function(a){var f,b;if(!a||"string"!=typeof a)return
 null;
try{v.DOMParser?(b=new DOMParser,f=b.parseFromString(a,"text/xml")):(f=new
 ActiveXObject("Microsoft.XMLDOM"),f.async="false",f.loadXML(a))}catch(t){f=p
}return f&&f.documentElement&&!f.getElementsByTagName("parsererror").length||c
.error("Invalid XML: "+a),f},noop:function(){},globalEval:function(a){a&&c
.trim(a)&&(v.execScript||function(a){v.eval.call(v,a)})(a)},camelCase:function
(a){return a.replace(Zb,"ms-").replace($b,ac)},nodeName:function(a,f){return a
.nodeName&&a.nodeName.toLowerCase()===f.toLowerCase()},
each:function(a,f,c){var b,g=0,l=a.length,u=m(a);if(c)if(u)for(;l>g&&(b=f
.apply(a[g],c),!1!==b);g++);else for(g in a){if(b=f.apply(a[g],c),!1===b)break
}else if(u)for(;l>g&&(b=f.call(a[g],g,a[g]),!1!==b);g++);else for(g in a)if
(b=f.call(a[g],g,a[g]),!1===b)break;return a},trim:Za&&!Za.call("\ufeff\u00a0"
)?function(a){return null==a?"":Za.call(a)}:function(a){return null==a?"":
(a+"").replace(Tb,"")},makeArray:function(a,f){var b=f||[];return null!=a&&(m
(Object(a))?c.merge(b,"string"==typeof a?[a]:a):
Xa.call(b,a)),b},inArray:function(a,f,c){var b;if(f){if(ob)return ob.call(f,a
,c);b=f.length;for(c=c?0>c?Math.max(0,b+c):c:0;b>c;c++)if(c in f&&f[c]===a
)return c}return-1},merge:function(a,f){var c=f.length,b=a.length,g=0;if
("number"==typeof c)for(;c>g;g++)a[b++]=f[g];else for(;f[g]!==p;)a[b++]=f[g++]
;return a.length=b,a},grep:function(a,f,c){var b,g=[],l=0,u=a.length;for(c=!!c
;u>l;l++)b=!!f(a[l],l),c!==b&&g.push(a[l]);return g},map:function(a,f,c){var b
,g=0,l=a.length;b=m(a);var u=[];if(b)for(;l>
g;g++)b=f(a[g],g,c),null!=b&&(u[u.length]=b);else for(g in a)b=f(a[g],g,c)
,null!=b&&(u[u.length]=b);return Oa.apply([],u)},guid:1,proxy:function(a,f)
{var b,t,g;return"string"==typeof f&&(b=a[f],f=a,a=b),c.isFunction(a)?(t=aa
.call(arguments,2),g=function(){return a.apply(f||this,t.concat(aa.call
(arguments)))},g.guid=a.guid=a.guid||c.guid++,g):p},access:function(a,f,b,t,g
,l,u){var x=0,e=a.length,d=null==b;if("object"===c.type(b))for(x in g=!0,b)c
.access(a,f,x,b[x],!0,l,u);else if(t!==p&&(g=!0,c.isFunction(t)||
(u=!0),d&&(u?(f.call(a,t),f=null):(d=f,f=function(a,f,b){return d.call(c(a),b)
})),f))for(;e>x;x++)f(a[x],b,u?t:t.call(a[x],x,f(a[x],b)));return g?a:d?f.call
(a):e?f(a[0],b):l},now:function(){return(new Date).getTime()}});c.ready
.promise=function(a){if(!G)if(G=c.Deferred(),"complete"===y.readyState
)setTimeout(c.ready);else if(y.addEventListener)y.addEventListener
("DOMContentLoaded",Qa,!1),v.addEventListener("load",c.ready,!1);else{y
.attachEvent("onreadystatechange",Qa);v.attachEvent("onload",c.ready);
var f=!1;try{f=null==v.frameElement&&y.documentElement}catch(b){}f&&f.doScroll
&&function g(){if(!c.isReady){try{f.doScroll("left")}catch(a){return
 setTimeout(g,50)}c.ready()}}()}return G.promise(a)};c.each("Boolean Number
 String Function Array Date RegExp Object Error".split(" "),function(a,f){ha["
[object "+f+"]"]=f.toLowerCase()});O=c(y);var kb={};c.Callbacks=function(a)
{a="string"==typeof a?kb[a]||d(a):c.extend({},a);var f,b,t,g,l,u,x=[],e=!a
.once&&[],q=function(c){f=a.memory&&c;b=!0;u=g||0;g=0;
l=x.length;for(t=!0;x&&l>u;u++)if(!1===x[u].apply(c[0],c[1])&&a.stopOnFalse)
{f=!1;break}t=!1;x&&(e?e.length&&q(e.shift()):f?x=[]:h.disable())},h=
{add:function(){if(x){var b=x.length;(function Pb(f){c.each(f,function(f,b)
{var k=c.type(b);"function"===k?a.unique&&h.has(b)||x.push(b):b&&b.length&
&"string"!==k&&Pb(b)})})(arguments);t?l=x.length:f&&(g=b,q(f))}return this}
,remove:function(){return x&&c.each(arguments,function(a,f){for(var b;-1<(b=c
.inArray(f,x,b));)x.splice(b,1),t&&(l>=b&&l--,u>=b&&u--)}),
this},has:function(a){return-1<c.inArray(a,x)},empty:function(){return x=[]
,this},disable:function(){return x=e=f=p,this},disabled:function(){return!x}
,lock:function(){return e=p,f||h.disable(),this},locked:function(){return!e}
,fireWith:function(a,f){return f=f||[],f=[a,f.slice?f.slice():f],!x||b&&!e||
(t?e.push(f):q(f)),this},fire:function(){return h.fireWith(this,arguments)
,this},fired:function(){return!!b}};return h};c.extend({Deferred:function(a)
{var f=[["resolve","done",c.Callbacks("once memory"),
"resolved"],["reject","fail",c.Callbacks("once memory"),"rejected"],["notify"
,"progress",c.Callbacks("memory")]],b="pending",t={state:function(){return b}
,always:function(){return g.done(arguments).fail(arguments),this}
,then:function(){var a=arguments;return c.Deferred(function(b){c.each(f
,function(f,k){var e=k[0],d=c.isFunction(a[f])&&a[f];g[k[1]](function(){var
 a=d&&d.apply(this,arguments);a&&c.isFunction(a.promise)?a.promise().done(b
.resolve).fail(b.reject).progress(b.notify):b[e+"With"](this===
t?b.promise():this,d?[a]:arguments)})});a=null}).promise()},promise:function(a
){return null!=a?c.extend(a,t):t}},g={};return t.pipe=t.then,c.each(f,function
(a,c){var e=c[2],d=c[3];t[c[1]]=e.add;d&&e.add(function(){b=d},f[1^a][2]
.disable,f[2][2].lock);g[c[0]]=function(){return g[c[0]+"With"]
(this===g?t:this,arguments),this};g[c[0]+"With"]=e.fireWith}),t.promise(g),a&
&a.call(g,g),g},when:function(a){var f,b,t,g=0,l=aa.call(arguments),u=l.length
,e=1!==u||a&&c.isFunction(a.promise)?u:0,d=1===e?a:c.Deferred(),
q=function(a,c,b){return function(k){c[a]=this;b[a]=1<arguments.length?aa.call
(arguments):k;b===f?d.notifyWith(c,b):--e||d.resolveWith(c,b)}};if(1<u)for
(f=Array(u),b=Array(u),t=Array(u);u>g;g++)l[g]&&c.isFunction(l[g].promise)?l[g
].promise().done(q(g,t,l)).fail(d.reject).progress(q(g,b,f)):--e;return e||d
.resolveWith(t,l),d.promise()}});c.support=function(){var a,f,b,t,g,l,u,e=y
.createElement("div");if(e.setAttribute("className","t"),e.innerHTML=
" <link/><table></table><a href='/a'>a</a><input type='checkbox'/>",
f=e.getElementsByTagName("*"),b=e.getElementsByTagName("a")[0],!f||!b||!f
.length)return{};t=y.createElement("select");g=t.appendChild(y.createElement
("option"));f=e.getElementsByTagName("input")[0];b.style.cssText="top:1px
;float:left;opacity:.5";a={getSetAttribute:"t"!==e.className
,leadingWhitespace:3===e.firstChild.nodeType,tbody:!e.getElementsByTagName
("tbody").length,htmlSerialize:!!e.getElementsByTagName("link").length
,style:/top/.test(b.getAttribute("style")),hrefNormalized:"/a"===b
.getAttribute("href"),
opacity:/^0.5/.test(b.style.opacity),cssFloat:!!b.style.cssFloat,checkOn:!!f
.value,optSelected:g.selected,enctype:!!y.createElement("form").enctype
,html5Clone:"<:nav></:nav>"!==y.createElement("nav").cloneNode(!0).outerHTML
,boxModel:"CSS1Compat"===y.compatMode,deleteExpando:!0,noCloneEvent:!0
,inlineBlockNeedsLayout:!1,shrinkWrapBlocks:!1,reliableMarginRight:!0
,boxSizingReliable:!0,pixelPosition:!1};f.checked=!0;a.noCloneChecked=f
.cloneNode(!0).checked;t.disabled=!0;a.optDisabled=!g.disabled;try{delete e
.test}catch(d){a.deleteExpando=
!1}f=y.createElement("input");f.setAttribute("value","");a.input=""===f
.getAttribute("value");f.value="t";f.setAttribute("type","radio");a
.radioValue="t"===f.value;f.setAttribute("checked","t");f.setAttribute("name"
,"t");b=y.createDocumentFragment();b.appendChild(f);a.appendChecked=f.checked
;a.checkClone=b.cloneNode(!0).cloneNode(!0).lastChild.checked;e.attachEvent&&
(e.attachEvent("onclick",function(){a.noCloneEvent=!1}),e.cloneNode(!0).click(
));for(u in{submit:!0,change:!0,focusin:!0})e.setAttribute(b=
"on"+u,"t"),a[u+"Bubbles"]=b in v||!1===e.attributes[b].expando;return e.style
.backgroundClip="content-box",e.cloneNode(!0).style.backgroundClip="",a
.clearCloneStyle="content-box"===e.style.backgroundClip,c(function(){var f,b,c
,k=y.getElementsByTagName("body")[0];k&&(f=y.createElement("div"),f.style
.cssText="border:0;width:0;height:0;position:absolute;top:0;left:-9999px
;margin-top:1px",k.appendChild(f).appendChild(e),e
.innerHTML="<table><tr><td></td><td>t</td></tr></table>",c=e
.getElementsByTagName("td"),
c[0].style.cssText="padding:0;margin:0;border:0;display:none",l=0===c[0]
.offsetHeight,c[0].style.display="",c[1].style.display="none",a
.reliableHiddenOffsets=l&&0===c[0].offsetHeight,e.innerHTML="",e.style
.cssText="box-sizing:border-box;-moz-box-sizing:border-box
;-webkit-box-sizing:border-box;padding:1px;border:1px;display:block;width:4px
;margin-top:1%;position:absolute;top:1%;",a.boxSizing=4===e.offsetWidth,a
.doesNotIncludeMarginInBodyOffset=1!==k.offsetTop,v.getComputedStyle&&(a
.pixelPosition="1%"!==
(v.getComputedStyle(e,null)||{}).top,a.boxSizingReliable="4px"===(v
.getComputedStyle(e,null)||{width:"4px"}).width,b=e.appendChild(y
.createElement("div")),b.style.cssText=e.style.cssText="padding:0;margin:0
;border:0;display:block;box-sizing:content-box;-moz-box-sizing:content-box
;-webkit-box-sizing:content-box;",b.style.marginRight=b.style.width="0",e
.style.width="1px",a.reliableMarginRight=!parseFloat((v.getComputedStyle(b
,null)||{}).marginRight)),e.style.zoom!==p&&(e.innerHTML="",e.style.cssText=
"padding:0;margin:0;border:0;display:block;box-sizing:content-box
;-moz-box-sizing:content-box;-webkit-box-sizing:content-box;width:1px
;padding:1px;display:inline;zoom:1",a.inlineBlockNeedsLayout=3===e.offsetWidth
,e.style.display="block",e.innerHTML="<div></div>",e.firstChild.style
.width="5px",a.shrinkWrapBlocks=3!==e.offsetWidth,k.style.zoom=1),k
.removeChild(f),e=null)}),f=t=b=g=b=f=null,a}();var Kb=/(?:\{[\s\S]*\}|\[[\s\S
]*\])$/,Jb=/([A-Z])/g;c.extend({cache:{},expando:"jQuery"+("1.9.0"+Math.random
()).replace(/\D/g,
""),noData:{embed:!0,object:"clsid:D27CDB6E-AE6D-11cf-96B8-444553540000"
,applet:!0},hasData:function(a){return a=a.nodeType?c.cache[a[c.expando]]:a[c
.expando],!!a&&!B(a)},data:function(a,f,b){return n(a,f,b,!1)}
,removeData:function(a,f){return h(a,f,!1)},_data:function(a,f,b){return n(a,f
,b,!0)},_removeData:function(a,f){return h(a,f,!0)},acceptData:function(a){var
 f=a.nodeName&&c.noData[a.nodeName.toLowerCase()];return!f||!0!==f&&a
.getAttribute("classid")===f}});c.fn.extend({data:function(a,f){var b,
t,g=this[0],l=0,e=null;if(a===p){if(this.length&&(e=c.data(g),1===g.nodeType&
&!c._data(g,"parsedAttrs"))){for(b=g.attributes;b.length>l;l++)t=b[l].name,t
.indexOf("data-")||(t=c.camelCase(t.substring(5)),r(g,t,e[t]));c._data(g
,"parsedAttrs",!0)}return e}return"object"==typeof a?this.each(function(){c
.data(this,a)}):c.access(this,function(f){return f===p?g?r(g,a,c.data(g,a)
):null:(this.each(function(){c.data(this,a,f)}),p)},null,f,1<arguments.length
,null,!0)},removeData:function(a){return this.each(function(){c.removeData
(this,
a)})}});c.extend({queue:function(a,f,b){var t;return a?(f=(f||"fx")+"queue"
,t=c._data(a,f),b&&(!t||c.isArray(b)?t=c._data(a,f,c.makeArray(b)):t.push(b))
,t||[]):p},dequeue:function(a,f){f=f||"fx";var b=c.queue(a,f),t=b.length,g=b
.shift(),l=c._queueHooks(a,f),e=function(){c.dequeue(a,f)};"inprogress"===g&&
(g=b.shift(),t--);(l.cur=g)&&("fx"===f&&b.unshift("inprogress"),delete l.stop
,g.call(a,e,l));!t&&l&&l.empty.fire()},_queueHooks:function(a,f){var
 b=f+"queueHooks";return c._data(a,b)||c._data(a,b,
{empty:c.Callbacks("once memory").add(function(){c._removeData(a,f+"queue");c
._removeData(a,b)})})}});c.fn.extend({queue:function(a,f){var b=2
;return"string"!=typeof a&&(f=a,a="fx",b--),b>arguments.length?c.queue(this[0]
,a):f===p?this:this.each(function(){var b=c.queue(this,a,f);c._queueHooks(this
,a);"fx"===a&&"inprogress"!==b[0]&&c.dequeue(this,a)})},dequeue:function(a)
{return this.each(function(){c.dequeue(this,a)})},delay:function(a,f){return
 a=c.fx?c.fx.speeds[a]||a:a,f=f||"fx",this.queue(f,function(f,
b){var c=setTimeout(f,a);b.stop=function(){clearTimeout(c)}})}
,clearQueue:function(a){return this.queue(a||"fx",[])},promise:function(a,f)
{var b,t=1,g=c.Deferred(),l=this,e=this.length,d=function(){--t||g.resolveWith
(l,[l])};"string"!=typeof a&&(f=a,a=p);for(a=a||"fx";e--;)(b=c._data(l[e]
,a+"queueHooks"))&&b.empty&&(t++,b.empty.add(d));return d(),g.promise(f)}})
;var ma,qb,$a=/[\t\r\n]/g,bc=/\r/g,cc=/^(?:input|select|textarea|button|object
)$/i,dc=/^(?:a|area)$/i,rb=/^(?:checked|selected|autofocus|autoplay|async
|controls|defer|disabled|hidden|loop|multiple|open|readonly|required|scoped
)$/i,
ab=/^(?:checked|selected)$/i,ia=c.support.getSetAttribute,bb=c.support.input;c
.fn.extend({attr:function(a,f){return c.access(this,c.attr,a,f,1<arguments
.length)},removeAttr:function(a){return this.each(function(){c.removeAttr(this
,a)})},prop:function(a,f){return c.access(this,c.prop,a,f,1<arguments.length)}
,removeProp:function(a){return a=c.propFix[a]||a,this.each(function(){try{this
[a]=p,delete this[a]}catch(f){}})},addClass:function(a){var f,b,t,g,l,e=0
,d=this.length;f="string"==typeof a&&a;if(c.isFunction(a))return this.each
(function(f){c(this).addClass(a.call(this,
f,this.className))});if(f)for(f=(a||"").match(Z)||[];d>e;e++)if(b=this[e]
,t=1===b.nodeType&&(b.className?(" "+b.className+" ").replace($a," "):" "))
{for(l=0;g=f[l++];)0>t.indexOf(" "+g+" ")&&(t+=g+" ");b.className=c.trim(t)
}return this},removeClass:function(a){var f,b,t,g,l,e=0,d=this.length
;f=0===arguments.length||"string"==typeof a&&a;if(c.isFunction(a))return this
.each(function(f){c(this).removeClass(a.call(this,f,this.className))});if(f
)for(f=(a||"").match(Z)||[];d>e;e++)if(b=this[e],t=1===b.nodeType&&
(b.className?(" "+b.className+" ").replace($a," "):"")){for(l=0;g=f[l++];)for(
;0<=t.indexOf(" "+g+" ");)t=t.replace(" "+g+" "," ");b.className=a?c.trim(t
):""}return this},toggleClass:function(a,f){var b=typeof a,t="boolean"==typeof
 f;return c.isFunction(a)?this.each(function(b){c(this).toggleClass(a.call
(this,b,this.className,f),f)}):this.each(function(){if("string"===b)for(var g
,l=0,e=c(this),d=f,q=a.match(Z)||[];g=q[l++];)d=t?d:!e.hasClass(g),e
[d?"addClass":"removeClass"](g);else("undefined"===
b||"boolean"===b)&&(this.className&&c._data(this,"__className__",this
.className),this.className=this.className||!1===a?"":c._data(this
,"__className__")||"")})},hasClass:function(a){a=" "+a+" ";for(var f=0,b=this
.length;b>f;f++)if(1===this[f].nodeType&&0<=(" "+this[f].className+" ")
.replace($a," ").indexOf(a))return!0;return!1},val:function(a){var f,b,t
,g=this[0];if(arguments.length)return t=c.isFunction(a),this.each(function(b)
{var k,g=c(this);1===this.nodeType&&(k=t?a.call(this,b,g.val()):a,null==
k?k="":"number"==typeof k?k+="":c.isArray(k)&&(k=c.map(k,function(a){return
 null==a?"":a+""})),f=c.valHooks[this.type]||c.valHooks[this.nodeName
.toLowerCase()],f&&"set"in f&&f.set(this,k,"value")!==p||(this.value=k))});if
(g)return f=c.valHooks[g.type]||c.valHooks[g.nodeName.toLowerCase()],f&
&"get"in f&&(b=f.get(g,"value"))!==p?b:(b=g.value,"string"==typeof b?b.replace
(bc,""):null==b?"":b)}});c.extend({valHooks:{option:{get:function(a){var f=a
.attributes.value;return!f||f.specified?a.value:a.text}},
select:{get:function(a){for(var f,b=a.options,t=a.selectedIndex
,g="select-one"===a.type||0>t,l=g?null:[],e=g?t+1:b.length,d=0>t?e:g?t:0;e>d
;d++)if(f=b[d],!(!f.selected&&d!==t||(c.support.optDisabled?f
.disabled:null!==f.getAttribute("disabled"))||f.parentNode.disabled&&c
.nodeName(f.parentNode,"optgroup"))){if(a=c(f).val(),g)return a;l.push(a)
}return l},set:function(a,f){var b=c.makeArray(f);return c(a).find("option")
.each(function(){this.selected=0<=c.inArray(c(this).val(),b)}),b.length||(a
.selectedIndex=
-1),b}}},attr:function(a,f,b){var t,g,l,e=a.nodeType;if(a&&3!==e&&8!==e&&2!==e
)return a.getAttribute===p?c.prop(a,f,b):(l=1!==e||!c.isXMLDoc(a),l&&(f=f
.toLowerCase(),g=c.attrHooks[f]||(rb.test(f)?qb:ma)),b===p?g&&l&&"get"in g&
&null!==(t=g.get(a,f))?t:(a.getAttribute!==p&&(t=a.getAttribute(f))
,null==t?p:t):null!==b?g&&l&&"set"in g&&(t=g.set(a,b,f))!==p?t:(a.setAttribute
(f,b+""),b):(c.removeAttr(a,f),p))},removeAttr:function(a,f){var b,t,g=0,e=f&
&f.match(Z);if(e&&1===a.nodeType)for(;b=e[g++];)t=c.propFix[b]||
b,rb.test(b)?!ia&&ab.test(b)?a[c.camelCase("default-"+b)]=a[t]=!1:a[t]=!1:c
.attr(a,b,""),a.removeAttribute(ia?b:t)},attrHooks:{type:{set:function(a,f){if
(!c.support.radioValue&&"radio"===f&&c.nodeName(a,"input")){var b=a.value
;return a.setAttribute("type",f),b&&(a.value=b),f}}}},propFix:
{tabindex:"tabIndex",readonly:"readOnly","for":"htmlFor","class":"className"
,maxlength:"maxLength",cellspacing:"cellSpacing",cellpadding:"cellPadding"
,rowspan:"rowSpan",colspan:"colSpan",usemap:"useMap",frameborder:"frameBorder"
,
contenteditable:"contentEditable"},prop:function(a,f,b){var t,g,e,u=a.nodeType
;if(a&&3!==u&&8!==u&&2!==u)return e=1!==u||!c.isXMLDoc(a),e&&(f=c.propFix[f]|
|f,g=c.propHooks[f]),b!==p?g&&"set"in g&&(t=g.set(a,b,f))!==p?t:a[f]=b:g&
&"get"in g&&null!==(t=g.get(a,f))?t:a[f]},propHooks:{tabIndex:{get:function(a)
{var f=a.getAttributeNode("tabindex");return f&&f.specified?parseInt(f.value
,10):cc.test(a.nodeName)||dc.test(a.nodeName)&&a.href?0:p}}}});qb=
{get:function(a,f){var b=c.prop(a,f),t="boolean"==typeof b&&
a.getAttribute(f);return(b="boolean"==typeof b?bb&&ia?null!=t:ab.test(f)?a[c
.camelCase("default-"+f)]:!!t:a.getAttributeNode(f))&&!1!==b.value?f
.toLowerCase():p},set:function(a,f,b){return!1===f?c.removeAttr(a,b):bb&&ia|
|!ab.test(b)?a.setAttribute(!ia&&c.propFix[b]||b,b):a[c.camelCase("default-"+b
)]=a[b]=!0,b}};bb&&ia||(c.attrHooks.value={get:function(a,f){var b=a
.getAttributeNode(f);return c.nodeName(a,"input")?a.defaultValue:b&&b
.specified?b.value:p},set:function(a,f,b){return c.nodeName(a,"input")?
(a.defaultValue=f,p):ma&&ma.set(a,f,b)}});ia||(ma=c.valHooks.button=
{get:function(a,f){var b=a.getAttributeNode(f);return b&&("id"===f||"name"===f
||"coords"===f?""!==b.value:b.specified)?b.value:p},set:function(a,f,b){var
 c=a.getAttributeNode(b);return c||a.setAttributeNode(c=a.ownerDocument
.createAttribute(b)),c.value=f+="","value"===b||f===a.getAttribute(b)?f:p}},c
.attrHooks.contenteditable={get:ma.get,set:function(a,b,c){ma.set(a
,""===b?!1:b,c)}},c.each(["width","height"],function(a,b){c.attrHooks[b]=
c.extend(c.attrHooks[b],{set:function(a,c){return""===c?(a.setAttribute(b
,"auto"),c):p}})}));c.support.hrefNormalized||(c.each(["href","src","width"
,"height"],function(a,b){c.attrHooks[b]=c.extend(c.attrHooks[b],{get:function
(a){a=a.getAttribute(b,2);return null==a?p:a}})}),c.each(["href","src"]
,function(a,b){c.propHooks[b]={get:function(a){return a.getAttribute(b,4)}}}))
;c.support.style||(c.attrHooks.style={get:function(a){return a.style.cssText|
|p},set:function(a,b){return a.style.cssText=b+""}});
c.support.optSelected||(c.propHooks.selected=c.extend(c.propHooks.selected,
{get:function(a){a=a.parentNode;return a&&(a.selectedIndex,a.parentNode&&a
.parentNode.selectedIndex),null}}));c.support.enctype||(c.propFix
.enctype="encoding");c.support.checkOn||c.each(["radio","checkbox"],function()
{c.valHooks[this]={get:function(a){return null===a.getAttribute("value"
)?"on":a.value}}});c.each(["radio","checkbox"],function(){c.valHooks[this]=c
.extend(c.valHooks[this],{set:function(a,b){return c.isArray(b)?
a.checked=0<=c.inArray(c(a).val(),b):p}})});var cb=/^(?:input|select|textarea
)$/i,ec=/^key/,fc=/^(?:mouse|contextmenu)|click/,sb=/^(?:focusinfocus
|focusoutblur)$/,tb=/^([^.]*)(?:\.(.+)|)$/;c.event={global:{},add:function(a,b
,k,t,g){var e,u,d,q,h,m,n,w,r;if(h=3!==a.nodeType&&8!==a.nodeType&&c._data(a))
{k.handler&&(e=k,k=e.handler,g=e.selector);k.guid||(k.guid=c.guid++);(q=h
.events)||(q=h.events={});(u=h.handle)||(u=h.handle=function(a){return c===p|
|a&&c.event.triggered===a.type?p:c.event.dispatch.apply(u.elem,
arguments)},u.elem=a);b=(b||"").match(Z)||[""];for(h=b.length;h--;)d=tb.exec(b
[h])||[],w=m=d[1],r=(d[2]||"").split(".").sort(),d=c.event.special[w]||{},w=
(g?d.delegateType:d.bindType)||w,d=c.event.special[w]||{},m=c.extend({type:w
,origType:m,data:t,handler:k,guid:k.guid,selector:g,needsContext:g&&c.expr
.match.needsContext.test(g),namespace:r.join(".")},e),(n=q[w])||(n=q[w]=[],n
.delegateCount=0,d.setup&&!1!==d.setup.call(a,t,r,u)||(a.addEventListener?a
.addEventListener(w,u,!1):a.attachEvent&&a.attachEvent("on"+
w,u))),d.add&&(d.add.call(a,m),m.handler.guid||(m.handler.guid=k.guid)),g?n
.splice(n.delegateCount++,0,m):n.push(m),c.event.global[w]=!0;a=null}}
,remove:function(a,b,k,e,g){var l,u,d,q,h,m,n,w,p,r,z,v=c.hasData(a)&&c._data
(a);if(v&&(q=v.events)){b=(b||"").match(Z)||[""];for(h=b.length;h--;)if(d=tb
.exec(b[h])||[],p=z=d[1],r=(d[2]||"").split(".").sort(),p){n=c.event.special[p
]||{};p=(e?n.delegateType:n.bindType)||p;w=q[p]||[];d=d[2]&&RegExp("(^|\\.)"+r
.join("\\.(?:.*\\.|)")+"(\\.|$)");for(u=l=w.length;l--;)m=
w[l],!g&&z!==m.origType||k&&k.guid!==m.guid||d&&!d.test(m.namespace)||e&&e!==m
.selector&&("**"!==e||!m.selector)||(w.splice(l,1),m.selector&&w
.delegateCount--,n.remove&&n.remove.call(a,m));u&&!w.length&&(n.teardown&
&!1!==n.teardown.call(a,r,v.handle)||c.removeEvent(a,p,v.handle),delete q[p])
}else for(p in q)c.event.remove(a,p+b[h],k,e,!0);c.isEmptyObject(q)&&(delete v
.handle,c._removeData(a,"events"))}},trigger:function(a,b,k,e){var g,l,d,q,h,m
,n=[k||y],w=a.type||a;h=a.namespace?a.namespace.split("."):
[];if(l=g=k=k||y,3!==k.nodeType&&8!==k.nodeType&&!sb.test(w+c.event.triggered)
&&(0<=w.indexOf(".")&&(h=w.split("."),w=h.shift(),h.sort()),q=0>w.indexOf(":")
&&"on"+w,a=a[c.expando]?a:new c.Event(w,"object"==typeof a&&a),a.isTrigger=!0
,a.namespace=h.join("."),a.namespace_re=a.namespace?RegExp("(^|\\.)"+h.join
("\\.(?:.*\\.|)")+"(\\.|$)"):null,a.result=p,a.target||(a.target=k),b=null==b?
[a]:c.makeArray(b,[a]),m=c.event.special[w]||{},e||!m.trigger||!1!==m.trigger
.apply(k,b))){if(!e&&!m.noBubble&&!c.isWindow(k)){d=
m.delegateType||w;for(sb.test(d+w)||(l=l.parentNode);l;l=l.parentNode)n.push(l
),g=l;g===(k.ownerDocument||y)&&n.push(g.defaultView||g.parentWindow||v)}for
(g=0;(l=n[g++])&&!a.isPropagationStopped();)a.type=1<g?d:m.bindType||w,(h=(c
._data(l,"events")||{})[a.type]&&c._data(l,"handle"))&&h.apply(l,b),(h=q&&l[q]
)&&c.acceptData(l)&&h.apply&&!1===h.apply(l,b)&&a.preventDefault();if(a.type=w
,!(e||a.isDefaultPrevented()||m._default&&!1!==m._default.apply(k
.ownerDocument,b)||"click"===w&&c.nodeName(k,"a"))&&
c.acceptData(k)&&q&&k[w]&&!c.isWindow(k)){(g=k[q])&&(k[q]=null);c.event
.triggered=w;try{k[w]()}catch(r){}c.event.triggered=p;g&&(k[q]=g)}return a
.result}},dispatch:function(a){a=c.event.fix(a);var b,k,e,g,l,d=[],q=aa.call
(arguments);b=(c._data(this,"events")||{})[a.type]||[];var h=c.event.special[a
.type]||{};if(q[0]=a,a.delegateTarget=this,!h.preDispatch||!1!==h.preDispatch
.call(this,a)){d=c.event.handlers.call(this,a,b);for(b=0;(g=d[b++])&&!a
.isPropagationStopped();)for(a.currentTarget=g.elem,k=
0;(l=g.handlers[k++])&&!a.isImmediatePropagationStopped();)a.namespace_re&&!a
.namespace_re.test(l.namespace)||(a.handleObj=l,a.data=l.data,e=((c.event
.special[l.origType]||{}).handle||l.handler).apply(g.elem,q),e===p||!1!==(a
.result=e)||(a.preventDefault(),a.stopPropagation()));return h.postDispatch&&h
.postDispatch.call(this,a),a.result}},handlers:function(a,b){var k,e,g,l,d=[]
,q=b.delegateCount,h=a.target;if(q&&h.nodeType&&(!a.button||"click"!==a.type)
)for(;h!=this;h=h.parentNode||this)if(!0!==h.disabled||
"click"!==a.type){e=[];for(k=0;q>k;k++)l=b[k],g=l.selector+" ",e[g]===p&&(e[g
]=l.needsContext?0<=c(g,this).index(h):c.find(g,this,null,[h]).length),e[g]&&e
.push(l);e.length&&d.push({elem:h,handlers:e})}return b.length>q&&d.push(
{elem:this,handlers:b.slice(q)}),d},fix:function(a){if(a[c.expando])return a
;var b,k,e=a,g=c.event.fixHooks[a.type]||{},l=g.props?this.props.concat(g
.props):this.props;a=new c.Event(e);for(b=l.length;b--;)k=l[b],a[k]=e[k]
;return a.target||(a.target=e.srcElement||y),3===a.target.nodeType&&
(a.target=a.target.parentNode),a.metaKey=!!a.metaKey,g.filter?g.filter(a,e):a}
,props:"altKey bubbles cancelable ctrlKey currentTarget eventPhase metaKey
 relatedTarget shiftKey target timeStamp view which".split(" "),fixHooks:{}
,keyHooks:{props:["char","charCode","key","keyCode"],filter:function(a,b)
{return null==a.which&&(a.which=null!=b.charCode?b.charCode:b.keyCode),a}}
,mouseHooks:{props:"button buttons clientX clientY fromElement offsetX offsetY
 pageX pageY screenX screenY toElement".split(" "),
filter:function(a,b){var c,e,g,l=b.button,d=b.fromElement;return null==a.pageX
&&null!=b.clientX&&(c=a.target.ownerDocument||y,e=c.documentElement,g=c.body,a
.pageX=b.clientX+(e&&e.scrollLeft||g&&g.scrollLeft||0)-(e&&e.clientLeft||g&&g
.clientLeft||0),a.pageY=b.clientY+(e&&e.scrollTop||g&&g.scrollTop||0)-(e&&e
.clientTop||g&&g.clientTop||0)),!a.relatedTarget&&d&&(a.relatedTarget=d===a
.target?b.toElement:d),a.which||l===p||(a.which=1&l?1:2&l?3:4&l?2:0),a}}
,special:{load:{noBubble:!0},click:{trigger:function(){return c.nodeName(this,
"input")&&"checkbox"===this.type&&this.click?(this.click(),!1):p}},focus:
{trigger:function(){if(this!==y.activeElement&&this.focus)try{return this
.focus(),!1}catch(a){}},delegateType:"focusin"},blur:{trigger:function()
{return this===y.activeElement&&this.blur?(this.blur(),!1):p}
,delegateType:"focusout"},beforeunload:{postDispatch:function(a){a.result!==p&
&(a.originalEvent.returnValue=a.result)}}},simulate:function(a,b,k,e){a=c
.extend(new c.Event,k,{type:a,isSimulated:!0,originalEvent:{}});e?c.event
.trigger(a,
null,b):c.event.dispatch.call(b,a);a.isDefaultPrevented()&&k.preventDefault()}
};c.removeEvent=y.removeEventListener?function(a,b,c){a.removeEventListener&&a
.removeEventListener(b,c,!1)}:function(a,b,c){b="on"+b;a.detachEvent&&(a[b
]===p&&(a[b]=null),a.detachEvent(b,c))};c.Event=function(a,b){return this
 instanceof c.Event?(a&&a.type?(this.originalEvent=a,this.type=a.type,this
.isDefaultPrevented=a.defaultPrevented||!1===a.returnValue||a
.getPreventDefault&&a.getPreventDefault()?D:M):this.type=a,b&&c.extend(this,
b),this.timeStamp=a&&a.timeStamp||c.now(),this[c.expando]=!0,p):new c.Event(a
,b)};c.Event.prototype={isDefaultPrevented:M,isPropagationStopped:M
,isImmediatePropagationStopped:M,preventDefault:function(){var a=this
.originalEvent;this.isDefaultPrevented=D;a&&(a.preventDefault?a.preventDefault
():a.returnValue=!1)},stopPropagation:function(){var a=this.originalEvent;this
.isPropagationStopped=D;a&&(a.stopPropagation&&a.stopPropagation(),a
.cancelBubble=!0)},stopImmediatePropagation:function(){this
.isImmediatePropagationStopped=
D;this.stopPropagation()}};c.each({mouseenter:"mouseover"
,mouseleave:"mouseout"},function(a,b){c.event.special[a]={delegateType:b
,bindType:b,handle:function(a){var e,g=this,l=a.relatedTarget,d=a.handleObj
;return(!l||l!==g&&!c.contains(g,l))&&(a.type=d.origType,e=d.handler.apply
(this,arguments),a.type=b),e}}});c.support.submitBubbles||(c.event.special
.submit={setup:function(){return c.nodeName(this,"form")?!1:(c.event.add(this
,"click._submit keypress._submit",function(a){a=a.target;(a=c.nodeName(a,
"input")||c.nodeName(a,"button")?a.form:p)&&!c._data(a,"submitBubbles")&&(c
.event.add(a,"submit._submit",function(a){a._submit_bubble=!0}),c._data(a
,"submitBubbles",!0))}),p)},postDispatch:function(a){a._submit_bubble&&(delete
 a._submit_bubble,this.parentNode&&!a.isTrigger&&c.event.simulate("submit"
,this.parentNode,a,!0))},teardown:function(){return c.nodeName(this,"form"
)?!1:(c.event.remove(this,"._submit"),p)}});c.support.changeBubbles||(c.event
.special.change={setup:function(){return cb.test(this.nodeName)?
(("checkbox"===this.type||"radio"===this.type)&&(c.event.add(this
,"propertychange._change",function(a){"checked"===a.originalEvent.propertyName
&&(this._just_changed=!0)}),c.event.add(this,"click._change",function(a){this
._just_changed&&!a.isTrigger&&(this._just_changed=!1);c.event.simulate
("change",this,a,!0)})),!1):(c.event.add(this,"beforeactivate._change"
,function(a){a=a.target;cb.test(a.nodeName)&&!c._data(a,"changeBubbles")&&(c
.event.add(a,"change._change",function(a){!this.parentNode||a.isSimulated||
a.isTrigger||c.event.simulate("change",this.parentNode,a,!0)}),c._data(a
,"changeBubbles",!0))}),p)},handle:function(a){var b=a.target;return this!==b|
|a.isSimulated||a.isTrigger||"radio"!==b.type&&"checkbox"!==b.type?a.handleObj
.handler.apply(this,arguments):p},teardown:function(){return c.event.remove
(this,"._change"),!cb.test(this.nodeName)}});c.support.focusinBubbles||c.each(
{focus:"focusin",blur:"focusout"},function(a,b){var k=0,e=function(a){c.event
.simulate(b,a.target,c.event.fix(a),!0)};c.event.special[b]=
{setup:function(){0===k++&&y.addEventListener(a,e,!0)},teardown:function()
{0===--k&&y.removeEventListener(a,e,!0)}}});c.fn.extend({on:function(a,b,k,e,g
){var l,d;if("object"==typeof a){"string"!=typeof b&&(k=k||b,b=p);for(d in a
)this.on(d,b,k,a[d],g);return this}if(null==k&&null==e?(e=b,k=b=p):null==e&&
("string"==typeof b?(e=k,k=p):(e=k,k=b,b=p)),!1===e)e=M;else if(!e)return this
;return 1===g&&(l=e,e=function(a){return c().off(a),l.apply(this,arguments)},e
.guid=l.guid||(l.guid=c.guid++)),this.each(function(){c.event.add(this,
a,e,k,b)})},one:function(a,b,c,e){return this.on(a,b,c,e,1)},off:function(a,b
,k){var e,g;if(a&&a.preventDefault&&a.handleObj)return e=a.handleObj,c(a
.delegateTarget).off(e.namespace?e.origType+"."+e.namespace:e.origType,e
.selector,e.handler),this;if("object"==typeof a){for(g in a)this.off(g,b,a[g])
;return this}return(!1===b||"function"==typeof b)&&(k=b,b=p),!1===k&&(k=M)
,this.each(function(){c.event.remove(this,a,k,b)})},bind:function(a,b,c)
{return this.on(a,null,b,c)},unbind:function(a,b){return this.off(a,
null,b)},delegate:function(a,b,c,e){return this.on(b,a,c,e)}
,undelegate:function(a,b,c){return 1===arguments.length?this.off(a,"**"):this
.off(b,a||"**",c)},trigger:function(a,b){return this.each(function(){c.event
.trigger(a,b,this)})},triggerHandler:function(a,b){var k=this[0];return k?c
.event.trigger(a,b,k,!0):p},hover:function(a,b){return this.mouseenter(a)
.mouseleave(b||a)}});c.each("blur focus focusin focusout load resize scroll
 unload click dblclick mousedown mouseup mousemove mouseover mouseout
 mouseenter mouseleave change select submit keydown keypress keyup error
 contextmenu".split(" "),
function(a,b){c.fn[b]=function(a,c){return 0<arguments.length?this.on(b,null,a
,c):this.trigger(b)};ec.test(b)&&(c.event.fixHooks[b]=c.event.keyHooks);fc
.test(b)&&(c.event.fixHooks[b]=c.event.mouseHooks)});(function(a,b){function k
(a){return ia.test(a+"")}function e(){var a,b=[];return a=function(c,f){return
 b.push(c+=" ")>A.cacheLength&&delete a[b.shift()],a[c]=f}}function g(a)
{return a[K]=!0,a}function l(a){var b=T.createElement("div");try{return a(b)
}catch(c){return!1}finally{}}function d(a,b,c,
f){var k,g,e,l,t;if((b?b.ownerDocument||b:qa)!==T&&F(b),b=b||T,c=c||[],!a|
|"string"!=typeof a)return c;if(1!==(l=b.nodeType)&&9!==l)return[];if(!ba&&!f)
{if(k=ja.exec(a))if(e=k[1])if(9===l){if(g=b.getElementById(e),!g||!g
.parentNode)return c;if(g.id===e)return c.push(g),c}else{if(b.ownerDocument&&
(g=b.ownerDocument.getElementById(e))&&ta(b,g)&&g.id===e)return c.push(g),c
}else{if(k[2])return Q.apply(c,R.call(b.getElementsByTagName(a),0)),c;if((e=k
[3])&&L.getByClassName&&b.getElementsByClassName)return Q.apply(c,
R.call(b.getElementsByClassName(e),0)),c}if(L.qsa&&!ca.test(a)){if(k=!0,g=K
,e=b,t=9===l&&a,1===l&&"object"!==b.nodeName.toLowerCase()){l=w(a);(k=b
.getAttribute("id"))?g=k.replace(ma,"\\$&"):b.setAttribute("id",g);g="
[id='"+g+"'] ";for(e=l.length;e--;)l[e]=g+p(l[e]);e=aa.test(a)&&b.parentNode|
|b;t=l.join(",")}if(t)try{return Q.apply(c,R.call(e.querySelectorAll(t),0)),c
}catch(q){}finally{k||b.removeAttribute("id")}}}var u;a:{a=a.replace(ha,"$1")
;var h,m,n;k=w(a);if(!f&&1===k.length){if(u=k[0]=k[0].slice(0),
2<u.length&&"ID"===(h=u[0]).type&&9===b.nodeType&&!ba&&A.relative[u[1].type])
{if(b=A.find.ID(h.matches[0].replace(na,oa),b)[0],!b){u=c;break a}a=a.slice(u
.shift().value.length)}for(l=Y.needsContext.test(a)?-1:u.length-1;0<=l&&(h=u[l
],!A.relative[m=h.type]);l--)if((n=A.find[m])&&(f=n(h.matches[0].replace(na,oa
),aa.test(u[0].type)&&b.parentNode||b))){if(u.splice(l,1),a=f.length&&p(u),!a)
{u=(Q.apply(c,R.call(f,0)),c);break a}break}}u=(M(a,k)(f,b,ba,c,aa.test(a)),c)
}return u}function q(a,b){for(var c=
a&&b&&a.nextSibling;c;c=c.nextSibling)if(c===b)return-1;return a?1:-1}function
 h(a){return function(b){var c=b.nodeName.toLowerCase();return"input"===c&&b
.type===a}}function m(a){return function(b){var c=b.nodeName.toLowerCase()
;return("input"===c||"button"===c)&&b.type===a}}function n(a){return g
(function(b){return b=+b,g(function(c,f){for(var k,g=a([],c.length,b),e=g
.length;e--;)c[k=g[e]]&&(c[k]=!(f[k]=c[k]))})})}function w(a,b){var c,f,k,g,e
,l,t;if(e=S[a+" "])return b?0:e.slice(0);e=a;l=[];for(t=
A.preFilter;e;){c&&!(f=da.exec(e))||(f&&(e=e.slice(f[0].length)||e),l.push(k=[
]));c=!1;(f=ea.exec(e))&&(c=f.shift(),k.push({value:c,type:f[0].replace(ha," "
)}),e=e.slice(c.length));for(g in A.filter)!(f=Y[g].exec(e))||t[g]&&!(f=t[g](f
))||(c=f.shift(),k.push({value:c,type:g,matches:f}),e=e.slice(c.length));if(!c
)break}return b?e.length:e?d.error(a):S(a,l).slice(0)}function p(a){for(var
 b=0,c=a.length,f="";c>b;b++)f+=a[b].value;return f}function r(a,b,c){var f=b
.dir,k=c&&"parentNode"===b.dir,g=Oa++;
return b.first?function(b,c,g){for(;b=b[f];)if(1===b.nodeType||k)return a(b,c
,g)}:function(b,c,e){var l,d,t,q=H+" "+g;if(e)for(;b=b[f];){if((1===b.nodeType
||k)&&a(b,c,e))return!0}else for(;b=b[f];)if(1===b.nodeType||k)if(t=b[K]||(b[K
]={}),(d=t[f])&&d[0]===q){if(!0===(l=d[1])||l===C)return!0===l}else if(d=t[f]=
[q],d[1]=a(b,c,e)||C,!0===d[1])return!0}}function z(a){return 1<a
.length?function(b,c,f){for(var k=a.length;k--;)if(!a[k](b,c,f))return!1
;return!0}:a[0]}function v(a,b,c,f,k){for(var g,e=[],
l=0,d=a.length,t=null!=b;d>l;l++)(g=a[l])&&(!c||c(g,f,k))&&(e.push(g),t&&b
.push(l));return e}function B(a,b,c,f,k,e){return f&&!f[K]&&(f=B(f)),k&&!k[K]&
&(k=B(k,e)),g(function(g,e,l,t){var q,h,m=[],n=[],x=e.length,w;if(!(w=g)){w=b|
|"*";for(var p=l.nodeType?[l]:l,r=[],z=0,Va=p.length;Va>z;z++)d(w,p[z],r);w=r
}w=!a||!g&&b?w:v(w,m,a,l,t);p=c?k||(g?a:x||f)?[]:e:w;if(c&&c(w,p,l,t),f)for
(q=v(p,n),f(q,[],l,t),l=q.length;l--;)(h=q[l])&&(p[n[l]]=!(w[n[l]]=h));if(g)
{if(k||a){if(k){q=[];for(l=p.length;l--;)(h=
p[l])&&q.push(w[l]=h);k(null,p=[],q,t)}for(l=p.length;l--;)(h=p[l])&&-1<
(q=k?Aa.call(g,h):m[l])&&(g[q]=!(e[q]=h))}}else p=v(p===e?p.splice(x,p.length
):p),k?k(null,e,p,t):Q.apply(e,p)})}function y(a){var b,c,f,k=a.length,g=A
.relative[a[0].type];c=g||A.relative[" "];for(var e=g?1:0,l=r(function(a)
{return a===b},c,!0),d=r(function(a){return-1<Aa.call(b,a)},c,!0),t=[function
(a,c,f){return!g&&(f||c!==Ra)||((b=c).nodeType?l(a,c,f):d(a,c,f))}];k>e;e++)if
(c=A.relative[a[e].type])t=[r(z(t),c)];else{if(c=A.filter[a[e].type].apply
(null,
a[e].matches),c[K]){for(f=++e;k>f&&!A.relative[a[f].type];f++);return B(1<e&&z
(t),1<e&&p(a.slice(0,e-1)).replace(ha,"$1"),c,f>e&&y(a.slice(e,f)),k>f&&y(a=a
.slice(f)),k>f&&p(a))}t.push(c)}return z(t)}function J(a,b){var c=0,f=0<b
.length,k=0<a.length,e=function(g,e,l,t,q){var h,m,n=[],w=0,p="0",x=g&&[]
,r=null!=q,z=Ra,Va=g||k&&A.find.TAG("*",q&&e.parentNode||e)
,B=H+=null==z?1:Math.E;for(r&&(Ra=e!==T&&e,C=c);null!=(q=Va[p]);p++){if(k&&q)
{for(h=0;m=a[h];h++)if(m(q,e,l)){t.push(q);break}r&&(H=B,C=++c)}f&&
((q=!m&&q)&&w--,g&&x.push(q))}if(w+=p,f&&p!==w){for(h=0;m=b[h];h++)m(x,n,e,l)
;if(g){if(0<w)for(;p--;)x[p]||n[p]||(n[p]=Z.call(t));n=v(n)}Q.apply(t,n);r&&!g
&&0<n.length&&1<w+b.length&&d.uniqueSort(t)}return r&&(H=B,Ra=z),x};return f?g
(e):e}function N(){}var O,C,A,D,G,M,E,Ra,F,T,W,ba,ca,ua,Sa,ta,V
,K="sizzle"+-new Date,qa=a.document,L={},H=0,Oa=0,Na=e(),S=e(),U=e(),I=typeof
 b,P=[],Z=P.pop,Q=P.push,R=P.slice,Aa=P.indexOf||function(a){for(var b=0
,c=this.length;c>b;b++)if(this[b]===a)return b;return-1},
P="(?:\\\\.|[\\w-]|[^\\x00-\\xa0])+".replace("w","w#"),ra="\\[
[\\x20\\t\\r\\n\\f]*((?:\\\\.|[\\w-]|[^\\x00-\\xa0])+)[\\x20\\t\\r\\n\\f]*(?:(
[*^$|!~]?=)[\\x20\\t\\r\\n\\f]*(?:(['\"])((?:\\\\.|[^\\\\])*?)\\3|("+P+")|)|)
[\\x20\\t\\r\\n\\f]*\\]",X=":((?:\\\\.|[\\w-]|[^\\x00-\\xa0])+)(?:\\(((['\"])(
(?:\\\\.|[^\\\\])*?)\\3|((?:\\\\.|[^\\\\()[\\]]|"+ra.replace(3,8)+")*)|.*)\\)|
)",ha=RegExp("^[\\x20\\t\\r\\n\\f]+|((?:^|[^\\\\])(?:\\\\.)*)
[\\x20\\t\\r\\n\\f]+$","g"),da=/^[\x20\t\r\n\f]*,[\x20\t\r\n\f]*/,ea=
/^[\x20\t\r\n\f]*([\x20\t\r\n\f>+~])[\x20\t\r\n\f]*/,fa=RegExp(X),ga=RegExp
("^"+P+"$"),Y={ID:/^#((?:\\.|[\w-]|[^\x00-\xa0])+)/,CLASS:/^\.((?:\\.|[\w-]|
[^\x00-\xa0])+)/,NAME:/^\[name=['"]?((?:\\.|[\w-]|[^\x00-\xa0])+)['"]?\]/
,TAG:RegExp("^("+"(?:\\\\.|[\\w-]|[^\\x00-\\xa0])+".replace("w","w*")+")")
,ATTR:RegExp("^"+ra),PSEUDO:RegExp("^"+X),CHILD:/^:(only|first|last|nth
|nth-last)-(child|of-type)(?:\([\x20\t\r\n\f]*(even|odd|(([+-]|)(\d*)n|)
[\x20\t\r\n\f]*(?:([+-]|)[\x20\t\r\n\f]*(\d+)|))[\x20\t\r\n\f]*\)|)/i,
needsContext:/^[\x20\t\r\n\f]*[>+~]|:(even|odd|eq|gt|lt|nth|first|last)(?:\(
[\x20\t\r\n\f]*((?:-\d)?\d*)[\x20\t\r\n\f]*\)|)(?=[^-]|$)/i},aa=/[\x20\t\r\n\f
]*[+~]/,ia=/\{\s*\[native code\]\s*\}/,ja=/^(?:#([\w-]+)|(\w+)|\.([\w-]+))$/
,ka=/^(?:input|select|textarea|button)$/i,la=/^h\d$/i,ma=/'|\\/g,pa=/\=
[\x20\t\r\n\f]*([^'"\]]*)[\x20\t\r\n\f]*\]/g,na=/\\([\da-fA-F]{1,6}
[\x20\t\r\n\f]?|.)/g,oa=function(a,b){var c="0x"+b-65536;return
 c!==c?b:0>c?String.fromCharCode(c+65536):String.fromCharCode(55296|c>>
10,56320|1023&c)};try{R.call(W.childNodes,0)[0].nodeType}catch(sa){R=function
(a){for(var b,c=[];b=this[a];a++)c.push(b);return c}}G=d.isXML=function(a)
{return(a=a&&(a.ownerDocument||a).documentElement)?"HTML"!==a.nodeName:!1};F=d
.setDocument=function(a){var c=a?a.ownerDocument||a:qa;return c!==T&&9===c
.nodeType&&c.documentElement?(T=c,W=c.documentElement,ba=G(c),L
.tagNameNoComments=l(function(a){return a.appendChild(c.createComment("")),!a
.getElementsByTagName("*").length}),L.attributes=l(function(a){a.innerHTML=
"<select></select>";a=typeof a.lastChild.getAttribute("multiple")
;return"boolean"!==a&&"string"!==a}),L.getByClassName=l(function(a){return a
.innerHTML="<div class='hidden e'></div><div class='hidden'></div>",a
.getElementsByClassName&&a.getElementsByClassName("e").length?(a.lastChild
.className="e",2===a.getElementsByClassName("e").length):!1}),L.getByName=l
(function(a){a.id=K+0;a.innerHTML="<a name='"+K+"'></a><div
 name='"+K+"'></div>";W.insertBefore(a,W.firstChild);var b=c.getElementsByName
&&c.getElementsByName(K).length===
2+c.getElementsByName(K+0).length;return L.getIdNotName=!c.getElementById(K),W
.removeChild(a),b}),A.attrHandle=l(function(a){return a.innerHTML="<a
 href='#'></a>",a.firstChild&&typeof a.firstChild.getAttribute!==I&&"#"===a
.firstChild.getAttribute("href")})?{}:{href:function(a){return a.getAttribute
("href",2)},type:function(a){return a.getAttribute("type")}},L.getIdNotName?(A
.find.ID=function(a,b){if(typeof b.getElementById!==I&&!ba){var c=b
.getElementById(a);return c&&c.parentNode?[c]:[]}},A.filter.ID=
function(a){var b=a.replace(na,oa);return function(a){return a.getAttribute
("id")===b}}):(A.find.ID=function(a,c){if(typeof c.getElementById!==I&&!ba)
{var k=c.getElementById(a);return k?k.id===a||typeof k.getAttributeNode!==I&&k
.getAttributeNode("id").value===a?[k]:b:[]}},A.filter.ID=function(a){var b=a
.replace(na,oa);return function(a){return(a=typeof a.getAttributeNode!==I&&a
.getAttributeNode("id"))&&a.value===b}}),A.find.TAG=L
.tagNameNoComments?function(a,c){return typeof c.getElementsByTagName!==
I?c.getElementsByTagName(a):b}:function(a,b){var c,f=[],k=0,g=b
.getElementsByTagName(a);if("*"===a){for(;c=g[k];k++)1===c.nodeType&&f.push(c)
;return f}return g},A.find.NAME=L.getByName&&function(a,c){return typeof c
.getElementsByName!==I?c.getElementsByName(name):b},A.find.CLASS=L
.getByClassName&&function(a,c){return typeof c.getElementsByClassName===I|
|ba?b:c.getElementsByClassName(a)},ua=[],ca=[":focus"],(L.qsa=k(c
.querySelectorAll))&&(l(function(a){a.innerHTML="<select><option
 selected=''></option></select>";
a.querySelectorAll("[selected]").length||ca.push("\\[[\\x20\\t\\r\\n\\f]*
(?:checked|disabled|ismap|multiple|readonly|selected|value)");a
.querySelectorAll(":checked").length||ca.push(":checked")}),l(function(a){a
.innerHTML="<input type='hidden' i=''/>";a.querySelectorAll("[i^='']").length&
&ca.push("[*^$]=[\\x20\\t\\r\\n\\f]*(?:\"\"|'')");a.querySelectorAll
(":enabled").length||ca.push(":enabled",":disabled");a.querySelectorAll("*,:x"
);ca.push(",.*:")})),(L.matchesSelector=k(Sa=W.matchesSelector||W
.mozMatchesSelector||
W.webkitMatchesSelector||W.oMatchesSelector||W.msMatchesSelector))&&l(function
(a){L.disconnectedMatch=Sa.call(a,"div");Sa.call(a,"[s!='']:x");ua.push("!=",X
)}),ca=RegExp(ca.join("|")),ua=RegExp(ua.join("|")),ta=k(W.contains)||W
.compareDocumentPosition?function(a,b){var c=9===a.nodeType?a
.documentElement:a,f=b&&b.parentNode;return a===f||!(!f||1!==f.nodeType||!(c
.contains?c.contains(f):a.compareDocumentPosition&&16&a
.compareDocumentPosition(f)))}:function(a,b){if(b)for(;b=b.parentNode;)if
(b===a)return!0;
return!1},V=W.compareDocumentPosition?function(a,b){var f;return a===b?(E=!0,0
):(f=b.compareDocumentPosition&&a.compareDocumentPosition&&a
.compareDocumentPosition(b))?1&f||a.parentNode&&11===a.parentNode
.nodeType?a===c||ta(qa,a)?-1:b===c||ta(qa,b)?1:0:4&f?-1:1:a
.compareDocumentPosition?-1:1}:function(a,b){var f,k=0;f=a.parentNode;var g=b
.parentNode,e=[a],l=[b];if(a===b)return E=!0,0;if(a.sourceIndex&&b.sourceIndex
)return(~b.sourceIndex||-2147483648)-(ta(qa,a)&&~a.sourceIndex||-2147483648)
;if(!f||
!g)return a===c?-1:b===c?1:f?-1:g?1:0;if(f===g)return q(a,b);for(f=a;f=f
.parentNode;)e.unshift(f);for(f=b;f=f.parentNode;)l.unshift(f);for(;e[k]===l[k
];)k++;return k?q(e[k],l[k]):e[k]===qa?-1:l[k]===qa?1:0},E=!1,[0,0].sort(V),L
.detectDuplicates=E,T):T};d.matches=function(a,b){return d(a,null,null,b)};d
.matchesSelector=function(a,b){if((a.ownerDocument||a)!==T&&F(a),b=b.replace
(pa,"='$1']"),!(!L.matchesSelector||ba||ua&&ua.test(b)||ca.test(b)))try{var
 c=Sa.call(a,b);if(c||L.disconnectedMatch||a.document&&
11!==a.document.nodeType)return c}catch(f){}return 0<d(b,T,null,[a]).length};d
.contains=function(a,b){return(a.ownerDocument||a)!==T&&F(a),ta(a,b)};d
.attr=function(a,b){var c;return(a.ownerDocument||a)!==T&&F(a),ba||(b=b
.toLowerCase()),(c=A.attrHandle[b])?c(a):ba||L.attributes?a.getAttribute(b):(
(c=a.getAttributeNode(b))||a.getAttribute(b))&&!0===a[b]?b:c&&c.specified?c
.value:null};d.error=function(a){throw Error("Syntax error, unrecognized
 expression: "+a);};d.uniqueSort=function(a){var b,c=[],f=
1,k=0;if(E=!L.detectDuplicates,a.sort(V),E){for(;b=a[f];f++)b===a[f-1]&&(k=c
.push(f));for(;k--;)a.splice(c[k],1)}return a};D=d.getText=function(a){var b
,c="",f=0;if(b=a.nodeType)if(1===b||9===b||11===b){if("string"==typeof a
.textContent)return a.textContent;for(a=a.firstChild;a;a=a.nextSibling)c+=D(a)
}else{if(3===b||4===b)return a.nodeValue}else for(;b=a[f];f++)c+=D(b);return c
};A=d.selectors={cacheLength:50,createPseudo:g,match:Y,find:{},relative:{">":
{dir:"parentNode",first:!0}," ":{dir:"parentNode"},
"+":{dir:"previousSibling",first:!0},"~":{dir:"previousSibling"}},preFilter:
{ATTR:function(a){return a[1]=a[1].replace(na,oa),a[3]=(a[4]||a[5]||"")
.replace(na,oa),"~="===a[2]&&(a[3]=" "+a[3]+" "),a.slice(0,4)},CHILD:function
(a){return a[1]=a[1].toLowerCase(),"nth"===a[1].slice(0,3)?(a[3]||d.error(a[0]
),a[4]=+(a[4]?a[5]+(a[6]||1):2*("even"===a[3]||"odd"===a[3])),a[5]=+(a[7]+a[8]
||"odd"===a[3])):a[3]&&d.error(a[0]),a},PSEUDO:function(a){var b,c=!a[5]&&a[2]
;return Y.CHILD.test(a[0])?null:(a[4]?a[2]=
a[4]:c&&fa.test(c)&&(b=w(c,!0))&&(b=c.indexOf(")",c.length-b)-c.length)&&(a[0
]=a[0].slice(0,b),a[2]=c.slice(0,b)),a.slice(0,3))}},filter:{TAG:function(a)
{return"*"===a?function(){return!0}:(a=a.replace(na,oa).toLowerCase(),function
(b){return b.nodeName&&b.nodeName.toLowerCase()===a})},CLASS:function(a){var
 b=Na[a+" "];return b||(b=RegExp("(^|[\\x20\\t\\r\\n\\f])"+a+"(
[\\x20\\t\\r\\n\\f]|$)"))&&Na(a,function(a){return b.test(a.className||typeof
 a.getAttribute!==I&&a.getAttribute("class")||"")})},ATTR:function(a,
b,c){return function(f){f=d.attr(f,a);return null==f?"!="===b:b?(f+=""
,"="===b?f===c:"!="===b?f!==c:"^="===b?c&&0===f.indexOf(c):"*="===b?c&&-1<f
.indexOf(c):"$="===b?c&&f.substr(f.length-c.length)===c:"~="===b?-1<(" "+f+" "
).indexOf(c):"|="===b?f===c||f.substr(0,c.length+1)===c+"-":!1):!0}}
,CHILD:function(a,b,c,f,k){var g="nth"!==a.slice(0,3),e="last"!==a.slice(-4)
,l="of-type"===b;return 1===f&&0===k?function(a){return!!a.parentNode
}:function(b,c,d){var t,q,u,h,m;c=g!==e?"nextSibling":"previousSibling";
var n=b.parentNode,w=l&&b.nodeName.toLowerCase();d=!d&&!l;if(n){if(g){for(;c;)
{for(q=b;q=q[c];)if(l?q.nodeName.toLowerCase()===w:1===q.nodeType)return!1
;m=c="only"===a&&!m&&"nextSibling"}return!0}if(m=[e?n.firstChild:n.lastChild]
,e&&d)for(d=n[K]||(n[K]={}),t=d[a]||[],h=t[0]===H&&t[1],u=t[0]===H&&t[2],q=h&
&n.childNodes[h];q=++h&&q&&q[c]||(u=h=0)||m.pop();){if(1===q.nodeType&&++u&
&q===b){d[a]=[H,h,u];break}}else if(d&&(t=(b[K]||(b[K]={}))[a])&&t[0]===H)u=t
[1];else for(;(q=++h&&q&&q[c]||(u=h=0)||m.pop())&&
((l?q.nodeName.toLowerCase()!==w:1!==q.nodeType)||!++u||(d&&((q[K]||(q[K]={}))
[a]=[H,u]),q!==b)););return u-=k,u===f||0===u%f&&0<=u/f}}},PSEUDO:function(a,b
){var c,f=A.pseudos[a]||A.setFilters[a.toLowerCase()]||d.error("unsupported
 pseudo: "+a);return f[K]?f(b):1<f.length?(c=[a,a,"",b],A.setFilters
.hasOwnProperty(a.toLowerCase())?g(function(a,c){for(var k,g=f(a,b),e=g.length
;e--;)k=Aa.call(a,g[e]),a[k]=!(c[k]=g[e])}):function(a){return f(a,0,c)}):f}}
,pseudos:{not:g(function(a){var b=[],c=[],f=M(a.replace(ha,
"$1"));return f[K]?g(function(a,b,c,k){var g;c=f(a,null,k,[]);for(k=a.length
;k--;)(g=c[k])&&(a[k]=!(b[k]=g))}):function(a,k,g){return b[0]=a,f(b,null,g,c)
,!c.pop()}}),has:g(function(a){return function(b){return 0<d(a,b).length}})
,contains:g(function(a){return function(b){return-1<(b.textContent||b
.innerText||D(b)).indexOf(a)}}),lang:g(function(a){return ga.test(a||"")||d
.error("unsupported lang: "+a),a=a.replace(na,oa).toLowerCase(),function(b)
{var c;do if(c=ba?b.getAttribute("xml:lang")||b.getAttribute("lang"):
b.lang)return c=c.toLowerCase(),c===a||0===c.indexOf(a+"-");while((b=b
.parentNode)&&1===b.nodeType);return!1}}),target:function(b){var c=a.location&
&a.location.hash;return c&&c.slice(1)===b.id},root:function(a){return a===W}
,focus:function(a){return a===T.activeElement&&(!T.hasFocus||T.hasFocus())&&!!
(a.type||a.href||~a.tabIndex)},enabled:function(a){return!1===a.disabled}
,disabled:function(a){return!0===a.disabled},checked:function(a){var b=a
.nodeName.toLowerCase();return"input"===b&&!!a.checked||
"option"===b&&!!a.selected},selected:function(a){return a.parentNode&&a
.parentNode.selectedIndex,!0===a.selected},empty:function(a){for(a=a
.firstChild;a;a=a.nextSibling)if("@"<a.nodeName||3===a.nodeType||4===a
.nodeType)return!1;return!0},parent:function(a){return!A.pseudos.empty(a)}
,header:function(a){return la.test(a.nodeName)},input:function(a){return ka
.test(a.nodeName)},button:function(a){var b=a.nodeName.toLowerCase()
;return"input"===b&&"button"===a.type||"button"===b},text:function(a){var b;
return"input"===a.nodeName.toLowerCase()&&"text"===a.type&&(null==(b=a
.getAttribute("type"))||b.toLowerCase()===a.type)},first:n(function(){return[0
]}),last:n(function(a,b){return[b-1]}),eq:n(function(a,b,c){return[0>c?c+b:c]}
),even:n(function(a,b){for(var c=0;b>c;c+=2)a.push(c);return a}),odd:n
(function(a,b){for(var c=1;b>c;c+=2)a.push(c);return a}),lt:n(function(a,b,c)
{for(b=0>c?c+b:c;0<=--b;)a.push(b);return a}),gt:n(function(a,b,c){for
(c=0>c?c+b:c;b>++c;)a.push(c);return a})}};for(O in{radio:!0,
checkbox:!0,file:!0,password:!0,image:!0})A.pseudos[O]=h(O);for(O in{submit:!0
,reset:!0})A.pseudos[O]=m(O);M=d.compile=function(a,b){var c,f=[],k=[],g=U[a+"
 "];if(!g){b||(b=w(a));for(c=b.length;c--;)g=y(b[c]),g[K]?f.push(g):k.push(g)
;g=U(a,J(k,f))}return g};A.pseudos.nth=A.pseudos.eq;A.filters=N.prototype=A
.pseudos;A.setFilters=new N;F();d.attr=c.attr;c.find=d;c.expr=d.selectors;c
.expr[":"]=c.expr.pseudos;c.unique=d.uniqueSort;c.text=d.getText;c.isXMLDoc=d
.isXML;c.contains=d.contains})(v);var gc=
/Until$/,hc=/^(?:parents|prev(?:Until|All))/,Lb=/^.[^:#\[\.,]*$/,ub=c.expr
.match.needsContext,ic={children:!0,contents:!0,next:!0,prev:!0};c.fn.extend(
{find:function(a){var b,k,e;if("string"!=typeof a)return e=this,this.pushStack
(c(a).filter(function(){for(b=0;e.length>b;b++)if(c.contains(e[b],this)
)return!0}));k=[];for(b=0;this.length>b;b++)c.find(a,this[b],k);return k=this
.pushStack(c.unique(k)),k.selector=(this.selector?this.selector+" ":"")+a,k}
,has:function(a){var b,k=c(a,this),e=k.length;return this.filter(function()
{for(b=
0;e>b;b++)if(c.contains(this,k[b]))return!0})},not:function(a){return this
.pushStack(A(this,a,!1))},filter:function(a){return this.pushStack(A(this,a,!0
))},is:function(a){return!!a&&("string"==typeof a?ub.test(a)?0<=c(a,this
.context).index(this[0]):0<c.filter(a,this).length:0<this.filter(a).length)}
,closest:function(a,b){for(var k,e=0,g=this.length,l=[],d=ub.test(a)|
|"string"!=typeof a?c(a,b||this.context):0;g>e;e++)for(k=this[e];k&&k
.ownerDocument&&k!==b&&11!==k.nodeType;){if(d?-1<d.index(k):c.find
.matchesSelector(k,
a)){l.push(k);break}k=k.parentNode}return this.pushStack(1<l.length?c.unique(l
):l)},index:function(a){return a?"string"==typeof a?c.inArray(this[0],c(a)):c
.inArray(a.jquery?a[0]:a,this):this[0]&&this[0].parentNode?this.first()
.prevAll().length:-1},add:function(a,b){var k="string"==typeof a?c(a,b):c
.makeArray(a&&a.nodeType?[a]:a),k=c.merge(this.get(),k);return this.pushStack
(c.unique(k))},addBack:function(a){return this.add(null==a?this
.prevObject:this.prevObject.filter(a))}});c.fn.andSelf=c.fn.addBack;
c.each({parent:function(a){return(a=a.parentNode)&&11!==a.nodeType?a:null}
,parents:function(a){return c.dir(a,"parentNode")},parentsUntil:function(a,b,k
){return c.dir(a,"parentNode",k)},next:function(a){return E(a,"nextSibling")}
,prev:function(a){return E(a,"previousSibling")},nextAll:function(a){return c
.dir(a,"nextSibling")},prevAll:function(a){return c.dir(a,"previousSibling")}
,nextUntil:function(a,b,k){return c.dir(a,"nextSibling",k)},prevUntil:function
(a,b,k){return c.dir(a,"previousSibling",
k)},siblings:function(a){return c.sibling((a.parentNode||{}).firstChild,a)}
,children:function(a){return c.sibling(a.firstChild)},contents:function(a)
{return c.nodeName(a,"iframe")?a.contentDocument||a.contentWindow.document:c
.merge([],a.childNodes)}},function(a,b){c.fn[a]=function(k,e){var g=c.map(this
,b,k);return gc.test(a)||(e=k),e&&"string"==typeof e&&(g=c.filter(e,g))
,g=1<this.length&&!ic[a]?c.unique(g):g,1<this.length&&hc.test(a)&&(g=g.reverse
()),this.pushStack(g)}});c.extend({filter:function(a,
b,k){return k&&(a=":not("+a+")"),1===b.length?c.find.matchesSelector(b[0],a)?
[b[0]]:[]:c.find.matches(a,b)},dir:function(a,b,k){var e=[];for(a=a[b];a&
&9!==a.nodeType&&(k===p||1!==a.nodeType||!c(a).is(k));)1===a.nodeType&&e.push
(a),a=a[b];return e},sibling:function(a,b){for(var c=[];a;a=a.nextSibling
)1===a.nodeType&&a!==b&&c.push(a);return c}});var lb="abbr|article|aside|audio
|bdi|canvas|data|datalist|details|figcaption|figure|footer|header|hgroup|mark
|meter|nav|output|progress|section|summary|time|video",
jc=/ jQuery\d+="(?:null|\d+)"/g,vb=RegExp("<(?:"+lb+")[\\s/>]","i"),db=/^\s+/
,wb=/<(?!area|br|col|embed|hr|img|input|link|meta|param)(([\w:]+)[^>]*)\/>/gi
,xb=/<([\w:]+)/,yb=/<tbody/i,kc=/<|&#?\w+;/,lc=/<(?:script|style|link)/i,Ua=/^
(?:checkbox|radio)$/i,mc=/checked\s*(?:[^=]|=\s*.checked.)/i,zb=/^$|\/(?:java
|ecma)script/i,Mb=/^true\/(.*)/,nc=/^\s*<!(?:\[CDATA\[|--)|(?:\]\]|--)>\s*$/g
,U={option:[1,"<select multiple='multiple'>","</select>"],legend:[1
,"<fieldset>","</fieldset>"],area:[1,"<map>","</map>"],
param:[1,"<object>","</object>"],thead:[1,"<table>","</table>"],tr:[2
,"<table><tbody>","</tbody></table>"],col:[2
,"<table><tbody></tbody><colgroup>","</colgroup></table>"],td:[3
,"<table><tbody><tr>","</tr></tbody></table>"],_default:c.support
.htmlSerialize?[0,"",""]:[1,"X<div>","</div>"]},oc=V(y),eb=oc.appendChild(y
.createElement("div"));U.optgroup=U.option;U.tbody=U.tfoot=U.colgroup=U
.caption=U.thead;U.th=U.td;c.fn.extend({text:function(a){return c.access(this
,function(a){return a===p?c.text(this):
this.empty().append((this[0]&&this[0].ownerDocument||y).createTextNode(a))}
,null,a,arguments.length)},wrapAll:function(a){if(c.isFunction(a))return this
.each(function(b){c(this).wrapAll(a.call(this,b))});if(this[0]){var b=c(a,this
[0].ownerDocument).eq(0).clone(!0);this[0].parentNode&&b.insertBefore(this[0])
;b.map(function(){for(var a=this;a.firstChild&&1===a.firstChild.nodeType;)a=a
.firstChild;return a}).append(this)}return this},wrapInner:function(a){return
 c.isFunction(a)?this.each(function(b){c(this).wrapInner(a.call(this,
b))}):this.each(function(){var b=c(this),k=b.contents();k.length?k.wrapAll(a
):b.append(a)})},wrap:function(a){var b=c.isFunction(a);return this.each
(function(k){c(this).wrapAll(b?a.call(this,k):a)})},unwrap:function(){return
 this.parent().each(function(){c.nodeName(this,"body")||c(this).replaceWith
(this.childNodes)}).end()},append:function(){return this.domManip(arguments,!0
,function(a){1!==this.nodeType&&11!==this.nodeType&&9!==this.nodeType||this
.appendChild(a)})},prepend:function(){return this.domManip(arguments,
!0,function(a){1!==this.nodeType&&11!==this.nodeType&&9!==this.nodeType||this
.insertBefore(a,this.firstChild)})},before:function(){return this.domManip
(arguments,!1,function(a){this.parentNode&&this.parentNode.insertBefore(a,this
)})},after:function(){return this.domManip(arguments,!1,function(a){this
.parentNode&&this.parentNode.insertBefore(a,this.nextSibling)})}
,remove:function(a,b){for(var k,e=0;null!=(k=this[e]);e++)(!a||0<c.filter(a,[k
]).length)&&(b||1!==k.nodeType||c.cleanData(F(k)),k.parentNode&&
(b&&c.contains(k.ownerDocument,k)&&R(F(k,"script")),k.parentNode.removeChild(k
)));return this},empty:function(){for(var a,b=0;null!=(a=this[b]);b++){for
(1===a.nodeType&&c.cleanData(F(a,!1));a.firstChild;)a.removeChild(a.firstChild
);a.options&&c.nodeName(a,"select")&&(a.options.length=0)}return this}
,clone:function(a,b){return a=null==a?!1:a,b=null==b?a:b,this.map(function()
{return c.clone(this,a,b)})},html:function(a){return c.access(this,function(a)
{var b=this[0]||{},e=0,g=this.length;if(a===p)return 1===
b.nodeType?b.innerHTML.replace(jc,""):p;if(!("string"!=typeof a||lc.test(a)|
|!c.support.htmlSerialize&&vb.test(a)||!c.support.leadingWhitespace&&db.test(a
)||U[(xb.exec(a)||["",""])[1].toLowerCase()])){a=a.replace(wb,"<$1></$2>");try
{for(;g>e;e++)b=this[e]||{},1===b.nodeType&&(c.cleanData(F(b,!1)),b
.innerHTML=a);b=0}catch(l){}}b&&this.empty().append(a)},null,a,arguments
.length)},replaceWith:function(a){var b=c.isFunction(a);return b|
|"string"==typeof a||(a=c(a).not(this).detach()),this.domManip([a],
!0,function(a){var b=this.nextSibling,f=this.parentNode;(f&&1===this.nodeType|
|11===this.nodeType)&&(c(this).remove(),b?b.parentNode.insertBefore(a,b):f
.appendChild(a))})},detach:function(a){return this.remove(a,!0)}
,domManip:function(a,b,k){a=Oa.apply([],a);var e,g,l,d,q=0,h=this.length
,m=this,n=h-1,w=a[0],r=c.isFunction(w);if(r||!(1>=h||"string"!=typeof w||c
.support.checkClone)&&mc.test(w))return this.each(function(c){var e=m.eq(c);r&
&(a[0]=w.call(this,c,b?e.html():p));e.domManip(a,b,k)});if(h&&
(e=c.buildFragment(a,this[0].ownerDocument,!1,this),g=e.firstChild,1===e
.childNodes.length&&(e=g),g)){b=b&&c.nodeName(g,"tr");g=c.map(F(e,"script"),Q)
;for(l=g.length;h>q;q++)d=e,q!==n&&(d=c.clone(d,!0,!0),l&&c.merge(g,F(d
,"script"))),k.call(b&&c.nodeName(this[q],"table")?ra(this[q],"tbody"):this[q]
,d,q);if(l)for(e=g[g.length-1].ownerDocument,c.map(g,ea),q=0;l>q;q++)d=g[q],zb
.test(d.type||"")&&!c._data(d,"globalEval")&&c.contains(e,d)&&(d.src?c.ajax(
{url:d.src,type:"GET",dataType:"script",async:!1,
global:!1,"throws":!0}):c.globalEval((d.text||d.textContent||d.innerHTML||"")
.replace(nc,"")));e=g=null}return this}});c.each({appendTo:"append"
,prependTo:"prepend",insertBefore:"before",insertAfter:"after"
,replaceAll:"replaceWith"},function(a,b){c.fn[a]=function(a){for(var e=0,g=[]
,l=c(a),d=l.length-1;d>=e;e++)a=e===d?this:this.clone(!0),c(l[e])[b](a),Xa
.apply(g,a.get());return this.pushStack(g)}});c.extend({clone:function(a,b,e)
{var d,g,l,q,h,m=c.contains(a.ownerDocument,a);if(c.support.html5Clone||
c.isXMLDoc(a)||!vb.test("<"+a.nodeName+">")?h=a.cloneNode(!0):(eb.innerHTML=a
.outerHTML,eb.removeChild(h=eb.firstChild)),!(c.support.noCloneEvent&&c
.support.noCloneChecked||1!==a.nodeType&&11!==a.nodeType||c.isXMLDoc(a)))for
(d=F(h),g=F(a),q=0;null!=(l=g[q]);++q)if(d[q]){var n=d[q],w=void 0,p=void 0
,r=void 0;if(1===n.nodeType){if(w=n.nodeName.toLowerCase(),!c.support
.noCloneEvent&&n[c.expando]){p=c._data(n);for(r in p.events)c.removeEvent(n,r
,p.handle);n.removeAttribute(c.expando)}"script"===w&&n.text!==
l.text?(Q(n).text=l.text,ea(n)):"object"===w?(n.parentNode&&(n.outerHTML=l
.outerHTML),c.support.html5Clone&&l.innerHTML&&!c.trim(n.innerHTML)&&(n
.innerHTML=l.innerHTML)):"input"===w&&Ua.test(l.type)?(n.defaultChecked=n
.checked=l.checked,n.value!==l.value&&(n.value=l.value)):"option"===w?n
.defaultSelected=n.selected=l.defaultSelected:("input"===w||"textarea"===w)&&
(n.defaultValue=l.defaultValue)}}if(b)if(e)for(g=g||F(a),d=d||F(h),q=0;null!=
(l=g[q]);q++)sa(l,d[q]);else sa(a,h);return d=F(h,"script"),
0<d.length&&R(d,!m&&F(a,"script")),h},buildFragment:function(a,b,e,d){for(var
 g,l,q,h,n,m,w,p=a.length,r=V(b),z=[],v=0;p>v;v++)if(l=a[v],l||0===l)if
("object"===c.type(l))c.merge(z,l.nodeType?[l]:l);else if(kc.test(l)){h=h||r
.appendChild(b.createElement("div"));q=(xb.exec(l)||["",""])[1].toLowerCase()
;n=U[q]||U._default;h.innerHTML=n[1]+l.replace(wb,"<$1></$2>")+n[2];for(w=n[0]
;w--;)h=h.lastChild;if(!c.support.leadingWhitespace&&db.test(l)&&z.push(b
.createTextNode(db.exec(l)[0])),!c.support.tbody)for(w=
(l="table"!==q||yb.test(l)?"<table>"!==n[1]||yb.test(l)?0:h:h.firstChild)&&l
.childNodes.length;w--;)c.nodeName(m=l.childNodes[w],"tbody")&&!m.childNodes
.length&&l.removeChild(m);c.merge(z,h.childNodes);for(h.textContent="";h
.firstChild;)h.removeChild(h.firstChild);h=r.lastChild}else z.push(b
.createTextNode(l));h&&r.removeChild(h);c.support.appendChecked||c.grep(F(z
,"input"),Da);for(v=0;l=z[v++];)if((!d||-1===c.inArray(l,d))&&(g=c.contains(l
.ownerDocument,l),h=F(r.appendChild(l),"script"),g&&R(h),
e))for(w=0;l=h[w++];)zb.test(l.type||"")&&e.push(l);return r}
,cleanData:function(a,b){for(var e,d,g,l,q=0,h=c.expando,n=c.cache,m=c.support
.deleteExpando,w=c.event.special;null!=(g=a[q]);q++)if((b||c.acceptData(g))&&
(d=g[h],e=d&&n[d])){if(e.events)for(l in e.events)w[l]?c.event.remove(g,l):c
.removeEvent(g,l,e.handle);n[d]&&(delete n[d],m?delete g[h]:g
.removeAttribute!==p?g.removeAttribute(h):g[h]=null,I.push(d))}}});var Y,ga,wa
,fb=/alpha\([^)]*\)/i,pc=/opacity\s*=\s*([^)]*)/,qc=/^(top|right|bottom|left
)$/,
rc=/^(none|table(?!-c[ea]).+)/,Ab=/^margin/,Nb=RegExp("^("+Pa+")(.*)$","i")
,Ia=RegExp("^("+Pa+")(?!px)[a-z%]+$","i"),sc=RegExp("^([+-])=("+Pa+")","i")
,nb={BODY:"block"},tc={position:"absolute",visibility:"hidden",display:"block"
},Bb={letterSpacing:0,fontWeight:400},fa=["Top","Right","Bottom","Left"],mb=
["Webkit","O","Moz","ms"];c.fn.extend({css:function(a,b){return c.access(this
,function(a,b,f){var e,d={},q=0;if(c.isArray(b)){f=ga(a);for(e=b.length;e>q
;q++)d[b[q]]=c.css(a,b[q],!1,f);return d}return f!==
p?c.style(a,b,f):c.css(a,b)},a,b,1<arguments.length)},show:function(){return X
(this,!0)},hide:function(){return X(this)},toggle:function(a){var
 b="boolean"==typeof a;return this.each(function(){(b?a:S(this))?c(this).show(
):c(this).hide()})}});c.extend({cssHooks:{opacity:{get:function(a,b){if(b){var
 c=Y(a,"opacity");return""===c?"1":c}}}},cssNumber:{columnCount:!0
,fillOpacity:!0,fontWeight:!0,lineHeight:!0,opacity:!0,orphans:!0,widows:!0
,zIndex:!0,zoom:!0},cssProps:{"float":c.support.cssFloat?"cssFloat":
"styleFloat"},style:function(a,b,e,d){if(a&&3!==a.nodeType&&8!==a.nodeType&&a
.style){var g,l,q,h=c.camelCase(b),n=a.style;if(b=c.cssProps[h]||(c.cssProps[h
]=Ea(n,h)),q=c.cssHooks[b]||c.cssHooks[h],e===p)return q&&"get"in q&&(g=q.get
(a,!1,d))!==p?g:n[b];if(l=typeof e,"string"===l&&(g=sc.exec(e))&&(e=(g[1]+1)*g
[2]+parseFloat(c.css(a,b)),l="number"),!(null==e||"number"===l&&isNaN(e)||
("number"!==l||c.cssNumber[h]||(e+="px"),c.support.clearCloneStyle||""!==e|
|0!==b.indexOf("background")||(n[b]="inherit"),
q&&"set"in q&&(e=q.set(a,e,d))===p)))try{n[b]=e}catch(m){}}},css:function(a,b
,e,d){var g,l,q,h=c.camelCase(b);return b=c.cssProps[h]||(c.cssProps[h]=Ea(a
.style,h)),q=c.cssHooks[b]||c.cssHooks[h],q&&"get"in q&&(g=q.get(a,!0,e))
,g===p&&(g=Y(a,b,d)),"normal"===g&&b in Bb&&(g=Bb[b]),e?(l=parseFloat(g)
,!0===e||c.isNumeric(l)?l||0:g):g},swap:function(a,b,c,e){var g,l={};for(g in
 b)l[g]=a.style[g],a.style[g]=b[g];c=c.apply(a,e||[]);for(g in b)a.style[g]=l
[g];return c}});v.getComputedStyle?(ga=function(a){return v.getComputedStyle(a
,
null)},Y=function(a,b,e){var d,g,l,q=(e=e||ga(a))?e.getPropertyValue(b)||e[b
]:p,h=a.style;return e&&(""!==q||c.contains(a.ownerDocument,a)||(q=c.style(a,b
)),Ia.test(q)&&Ab.test(b)&&(d=h.width,g=h.minWidth,l=h.maxWidth,h.minWidth=h
.maxWidth=h.width=q,q=e.width,h.width=d,h.minWidth=g,h.maxWidth=l)),q}):y
.documentElement.currentStyle&&(ga=function(a){return a.currentStyle}
,Y=function(a,b,c){var e,g,l;c=(c=c||ga(a))?c[b]:p;var d=a.style;return
 null==c&&d&&d[b]&&(c=d[b]),Ia.test(c)&&!qc.test(b)&&(e=d.left,
g=a.runtimeStyle,l=g&&g.left,l&&(g.left=a.currentStyle.left),d
.left="fontSize"===b?"1em":c,c=d.pixelLeft+"px",d.left=e,l&&(g.left=l))
,""===c?"auto":c});c.each(["height","width"],function(a,b){c.cssHooks[b]=
{get:function(a,e,g){return e?0===a.offsetWidth&&rc.test(c.css(a,"display"))?c
.swap(a,tc,function(){return Ha(a,b,g)}):Ha(a,b,g):p},set:function(a,e,g){var
 d=g&&ga(a);return va(a,e,g?Ga(a,b,g,c.support.boxSizing&&"border-box"===c.css
(a,"boxSizing",!1,d),d):0)}}});c.support.opacity||(c.cssHooks.opacity=
{get:function(a,b){return pc.test((b&&a.currentStyle?a.currentStyle.filter:a
.style.filter)||"")?.01*parseFloat(RegExp.$1)+"":b?"1":""},set:function(a,b)
{var e=a.style,d=a.currentStyle,g=c.isNumeric(b)?"alpha(opacity="+100*b+")":""
,l=d&&d.filter||e.filter||"";e.zoom=1;(1<=b||""===b)&&""===c.trim(l.replace(fb
,""))&&e.removeAttribute&&(e.removeAttribute("filter"),""===b||d&&!d.filter)||
(e.filter=fb.test(l)?l.replace(fb,g):l+" "+g)}});c(function(){c.support
.reliableMarginRight||(c.cssHooks.marginRight=
{get:function(a,b){return b?c.swap(a,{display:"inline-block"},Y,[a
,"marginRight"]):p}});!c.support.pixelPosition&&c.fn.position&&c.each(["top"
,"left"],function(a,b){c.cssHooks[b]={get:function(a,e){return e?(e=Y(a,b),Ia
.test(e)?c(a).position()[b]+"px":e):p}}})});c.expr&&c.expr.filters&&(c.expr
.filters.hidden=function(a){return 0===a.offsetWidth&&0===a.offsetHeight||!c
.support.reliableHiddenOffsets&&"none"===(a.style&&a.style.display||c.css(a
,"display"))},c.expr.filters.visible=function(a){return!c.expr.filters.hidden
(a)});
c.each({margin:"",padding:"",border:"Width"},function(a,b){c.cssHooks[a+b]=
{expand:function(c){var e=0,g={};for(c="string"==typeof c?c.split(" "):[c];4>e
;e++)g[a+fa[e]+b]=c[e]||c[e-2]||c[0];return g}};Ab.test(a)||(c.cssHooks[a+b]
.set=va)});var uc=/%20/g,Ob=/\[\]$/,Cb=/\r?\n/g,vc=/^(?:submit|button|image
|reset)$/i,wc=/^(?:input|select|textarea|keygen)/i;c.fn.extend(
{serialize:function(){return c.param(this.serializeArray())}
,serializeArray:function(){return this.map(function(){var a=c.prop(this
,"elements");
return a?c.makeArray(a):this}).filter(function(){var a=this.type;return this
.name&&!c(this).is(":disabled")&&wc.test(this.nodeName)&&!vc.test(a)&&(this
.checked||!Ua.test(a))}).map(function(a,b){var e=c(this).val();return
 null==e?null:c.isArray(e)?c.map(e,function(a){return{name:b.name,value:a
.replace(Cb,"\r\n")}}):{name:b.name,value:e.replace(Cb,"\r\n")}}).get()}});c
.param=function(a,b){var e,d=[],g=function(a,b){b=c.isFunction(b)?b(
):null==b?"":b;d[d.length]=encodeURIComponent(a)+"="+encodeURIComponent(b)};
if(b===p&&(b=c.ajaxSettings&&c.ajaxSettings.traditional),c.isArray(a)||a
.jquery&&!c.isPlainObject(a))c.each(a,function(){g(this.name,this.value)})
;else for(e in a)xa(e,a[e],b,g);return d.join("&").replace(uc,"+")};var ja,da
,gb=c.now(),hb=/\?/,xc=/#.*$/,Db=/([?&])_=[^&]*/,yc=/^(.*?):[ \t]*([^\r\n]*
)\r?$/gm,zc=/^(?:about|app|app-storage|.+-extension|file|res|widget):$/,Ac=/^
(?:GET|HEAD)$/,Bc=/^\/\//,Eb=/^([\w.+-]+:)(?:\/\/([^\/?#:]*)(?::(\d+)|)|)/
,Fb=c.fn.load,Gb={},Wa={},Hb="*/".concat("*");try{da=
Na.href}catch(Fc){da=y.createElement("a"),da.href="",da=da.href}ja=Eb.exec(da
.toLowerCase())||[];c.fn.load=function(a,b,e){if("string"!=typeof a&&Fb)return
 Fb.apply(this,arguments);var d,g,l,q=this,h=a.indexOf(" ");return 0<=h&&(d=a
.slice(h,a.length),a=a.slice(0,h)),c.isFunction(b)?(e=b,b=p):b&
&"object"==typeof b&&(g="POST"),0<q.length&&c.ajax({url:a,type:g
,dataType:"html",data:b}).done(function(a){l=arguments;q.html(d?c("<div>")
.append(c.parseHTML(a)).find(d):a)}).complete(e&&function(a,b){q.each(e,
l||[a.responseText,b,a])}),this};c.each("ajaxStart ajaxStop ajaxComplete
 ajaxError ajaxSuccess ajaxSend".split(" "),function(a,b){c.fn[b]=function(a)
{return this.on(b,a)}});c.each(["get","post"],function(a,b){c[b]=function(a,e
,g,d){return c.isFunction(e)&&(d=d||g,g=e,e=p),c.ajax({url:a,type:b,dataType:d
,data:e,success:g})}});c.extend({active:0,lastModified:{},etag:{}
,ajaxSettings:{url:da,type:"GET",isLocal:zc.test(ja[1]),global:!0
,processData:!0,async:!0,contentType:"application/x-www-form-urlencoded;
 charset=UTF-8",
accepts:{"*":Hb,text:"text/plain",html:"text/html",xml:"application/xml,
 text/xml",json:"application/json, text/javascript"},contents:{xml:/xml/
,html:/html/,json:/json/},responseFields:{xml:"responseXML"
,text:"responseText"},converters:{"* text":v.String,"text html":!0,"text
 json":c.parseJSON,"text xml":c.parseXML},flatOptions:{url:!0,context:!0}}
,ajaxSetup:function(a,b){return b?P(P(a,c.ajaxSettings),b):P(c.ajaxSettings,a)
},ajaxPrefilter:Ka(Gb),ajaxTransport:Ka(Wa),ajax:function(a,b){function e(a,
b,f,k){var q,n,w,u,J,N=b;if(2!==O){O=2;h&&clearTimeout(h);d=p;l=k||"";C
.readyState=0<a?4:0;if(f){var D;u=r;k=C;var G,E,M,F=u.contents,I=u.dataTypes
,L=u.responseFields;for(G in L)G in f&&(k[L[G]]=f[G]);for(;"*"===I[0];)I.shift
(),D===p&&(D=u.mimeType||k.getResponseHeader("Content-Type"));if(D)for(G in F
)if(F[G]&&F[G].test(D)){I.unshift(G);break}if(I[0]in f)E=I[0];else{for(G in f)
{if(!I[0]||u.converters[G+" "+I[0]]){E=G;break}M||(M=G)}E=E||M}u=D=E?(E!==I[0]
&&I.unshift(E),f[E]):p}if(200<=a&&300>a||304===
a)if(r.ifModified&&(J=C.getResponseHeader("Last-Modified"),J&&(c.lastModified
[g]=J),J=C.getResponseHeader("etag"),J&&(c.etag[g]=J)),304===a)q=!0
,N="notmodified";else{var H;a:{q=r;n=u;var Ca,V;J={};N=0;D=q.dataTypes.slice()
;G=D[0];if(q.dataFilter&&(n=q.dataFilter(n,q.dataType)),D[1])for(H in q
.converters)J[H.toLowerCase()]=q.converters[H];for(;w=D[++N];)if("*"!==w){if
("*"!==G&&G!==w){if(H=J[G+" "+w]||J["* "+w],!H)for(Ca in J)if(V=Ca.split(" ")
,V[1]===w&&(H=J[G+" "+V[0]]||J["* "+V[0]])){!0===H?H=J[Ca]:
!0!==J[Ca]&&(w=V[0],D.splice(N--,0,w));break}if(!0!==H)if(H&&q["throws"])n=H(n
);else try{n=H(n)}catch(P){H={state:"parsererror",error:H?P:"No conversion
 from "+G+" to "+w};break a}}G=w}H={state:"success",data:n}}q=H;N=q.state;n=q
.data;w=q.error;q=!w}else w=N,(a||!N)&&(N="error",0>a&&(a=0));C.status=a;C
.statusText=(b||N)+"";q?B.resolveWith(z,[n,N,C]):B.rejectWith(z,[C,N,w]);C
.statusCode(A);A=p;m&&v.trigger(q?"ajaxSuccess":"ajaxError",[C,r,q?n:w]);y
.fireWith(z,[C,N]);m&&(v.trigger("ajaxComplete",[C,
r]),--c.active||c.event.trigger("ajaxStop"))}}"object"==typeof a&&(b=a,a=p)
;b=b||{};var d,g,l,q,h,n,m,w,r=c.ajaxSetup({},b),z=r.context||r,v=r.context&&
(z.nodeType||z.jquery)?c(z):c.event,B=c.Deferred(),y=c.Callbacks("once memory"
),A=r.statusCode||{},J={},N={},O=0,D="canceled",C={readyState:0
,getResponseHeader:function(a){var b;if(2===O){if(!q)for(q={};b=yc.exec(l);)q
[b[1].toLowerCase()]=b[2];b=q[a.toLowerCase()]}return null==b?null:b}
,getAllResponseHeaders:function(){return 2===O?l:null}
,setRequestHeader:function(a,
b){var c=a.toLowerCase();return O||(a=N[c]=N[c]||a,J[a]=b),this}
,overrideMimeType:function(a){return O||(r.mimeType=a),this}
,statusCode:function(a){var b;if(a)if(2>O)for(b in a)A[b]=[A[b],a[b]];else C
.always(a[C.status]);return this},abort:function(a){a=a||D;return d&&d.abort(a
),e(0,a),this}};if(B.promise(C).complete=y.add,C.success=C.done,C.error=C.fail
,r.url=((a||r.url||da)+"").replace(xc,"").replace(Bc,ja[1]+"//"),r.type=b
.method||b.type||r.method||r.type,r.dataTypes=c.trim(r.dataType||"*")
.toLowerCase().match(Z)||
[""],null==r.crossDomain&&(n=Eb.exec(r.url.toLowerCase()),r.crossDomain=!(!n|
|n[1]===ja[1]&&n[2]===ja[2]&&(n[3]||("http:"===n[1]?80:443))==(ja[3]||
("http:"===ja[1]?80:443)))),r.data&&r.processData&&"string"!=typeof r.data&&(r
.data=c.param(r.data,r.traditional)),La(Gb,r,b,C),2===O)return C;(m=r.global)&
&0===c.active++&&c.event.trigger("ajaxStart");r.type=r.type.toUpperCase();r
.hasContent=!Ac.test(r.type);g=r.url;r.hasContent||(r.data&&(g=r.url+=(hb.test
(g)?"&":"?")+r.data,delete r.data),!1===r.cache&&
(r.url=Db.test(g)?g.replace(Db,"$1_="+gb++):g+(hb.test(g)?"&":"?")+"_="+gb++))
;r.ifModified&&(c.lastModified[g]&&C.setRequestHeader("If-Modified-Since",c
.lastModified[g]),c.etag[g]&&C.setRequestHeader("If-None-Match",c.etag[g]));(r
.data&&r.hasContent&&!1!==r.contentType||b.contentType)&&C.setRequestHeader
("Content-Type",r.contentType);C.setRequestHeader("Accept",r.dataTypes[0]&&r
.accepts[r.dataTypes[0]]?r.accepts[r.dataTypes[0]]+("*"!==r.dataTypes[0]?",
 "+Hb+"; q=0.01":""):r.accepts["*"]);for(w in r.headers)C.setRequestHeader(w,
r.headers[w]);if(r.beforeSend&&(!1===r.beforeSend.call(z,C,r)||2===O))return C
.abort();D="abort";for(w in{success:1,error:1,complete:1})C[w](r[w]);if(d=La
(Wa,r,b,C)){C.readyState=1;m&&v.trigger("ajaxSend",[C,r]);r.async&&0<r.timeout
&&(h=setTimeout(function(){C.abort("timeout")},r.timeout));try{O=1,d.send(J,e)
}catch(G){if(!(2>O))throw G;e(-1,G)}}else e(-1,"No Transport");return C}
,getScript:function(a,b){return c.get(a,p,b,"script")},getJSON:function(a,b,e)
{return c.get(a,b,e,"json")}});c.ajaxSetup({accepts:{script:"text/javascript,
 application/javascript, application/ecmascript, application/x-ecmascript"},
contents:{script:/(?:java|ecma)script/},converters:{"text script":function(a)
{return c.globalEval(a),a}}});c.ajaxPrefilter("script",function(a){a.cache===p
&&(a.cache=!1);a.crossDomain&&(a.type="GET",a.global=!1)});c.ajaxTransport
("script",function(a){if(a.crossDomain){var b,e=y.head||c("head")[0]||y
.documentElement;return{send:function(c,g){b=y.createElement("script");b
.async=!0;a.scriptCharset&&(b.charset=a.scriptCharset);b.src=a.url;b.onload=b
.onreadystatechange=function(a,c){(c||!b.readyState||
/loaded|complete/.test(b.readyState))&&(b.onload=b.onreadystatechange=null,b
.parentNode&&b.parentNode.removeChild(b),b=null,c||g(200,"success"))};e
.insertBefore(b,e.firstChild)},abort:function(){b&&b.onload(p,!0)}}}});var Ib=
[],ib=/(=)\?(?=&|$)|\?\?/;c.ajaxSetup({jsonp:"callback",jsonpCallback:function
(){var a=Ib.pop()||c.expando+"_"+gb++;return this[a]=!0,a}});c.ajaxPrefilter
("json jsonp",function(a,b,e){var d,g,l,q=!1!==a.jsonp&&(ib.test(a.url
)?"url":"string"==typeof a.data&&!(a.contentType||"").indexOf
("application/x-www-form-urlencoded")&&
ib.test(a.data)&&"data");return q||"jsonp"===a.dataTypes[0]?(d=a
.jsonpCallback=c.isFunction(a.jsonpCallback)?a.jsonpCallback():a.jsonpCallback
,q?a[q]=a[q].replace(ib,"$1"+d):!1!==a.jsonp&&(a.url+=(hb.test(a.url)?"&":"?"
)+a.jsonp+"="+d),a.converters["script json"]=function(){return l||c.error(d+"
 was not called"),l[0]},a.dataTypes[0]="json",g=v[d],v[d]=function()
{l=arguments},e.always(function(){v[d]=g;a[d]&&(a.jsonpCallback=b
.jsonpCallback,Ib.push(d));l&&c.isFunction(g)&&g(l[0]);l=g=p}),"script"):
p});var pa,Ba,Cc=0,jb=v.ActiveXObject&&function(){for(var a in pa)pa[a](p,!0)}
;c.ajaxSettings.xhr=v.ActiveXObject?function(){var a;if(!(a=!this.isLocal&&ka(
)))a:{try{a=new v.ActiveXObject("Microsoft.XMLHTTP");break a}catch(b){}a=void
 0}return a}:ka;Ba=c.ajaxSettings.xhr();c.support.cors=!!Ba&
&"withCredentials"in Ba;(Ba=c.support.ajax=!!Ba)&&c.ajaxTransport(function(a)
{if(!a.crossDomain||c.support.cors){var b;return{send:function(e,d){var g,l
,q=a.xhr();if(a.username?q.open(a.type,a.url,a.async,a.username,
a.password):q.open(a.type,a.url,a.async),a.xhrFields)for(l in a.xhrFields)q[l
]=a.xhrFields[l];a.mimeType&&q.overrideMimeType&&q.overrideMimeType(a.mimeType
);a.crossDomain||e["X-Requested-With"]||(e["X-Requested-With"
]="XMLHttpRequest");try{for(l in e)q.setRequestHeader(l,e[l])}catch(h){}q.send
(a.hasContent&&a.data||null);b=function(e,k){var l,h,n,w,m;try{if(b&&(k||4===q
.readyState))if(b=p,g&&(q.onreadystatechange=c.noop,jb&&delete pa[g]),k)4!==q
.readyState&&q.abort();else{w={};l=q.status;m=q.responseXML;
n=q.getAllResponseHeaders();m&&m.documentElement&&(w.xml=m);"string"==typeof q
.responseText&&(w.text=q.responseText);try{h=q.statusText}catch(r){h=""}l||!a
.isLocal||a.crossDomain?1223===l&&(l=204):l=w.text?200:404}}catch(z){k||d(-1,z
)}w&&d(l,h,w,n)};a.async?4===q.readyState?setTimeout(b):(g=++Cc,jb&&(pa||(pa={
},c(v).unload(jb)),pa[g]=b),q.onreadystatechange=b):b()},abort:function(){b&&b
(p,!0)}}}});var la,Ta,Qb=/^(?:toggle|show|hide)$/,Dc=RegExp("^(?:([+-])=|)
("+Pa+")([a-z%]*)$","i"),Ec=/queueHooks$/,
Ma=[w],za={"*":[function(a,b){var e,d,g=this.createTween(a,b),l=Dc.exec(b),q=g
.cur(),h=+q||0,n=1,w=20;if(l){if(e=+l[2],d=l[3]||(c.cssNumber[a]?"":"px")
,"px"!==d&&h){h=c.css(g.elem,a,!0)||e||1;do n=n||".5",h/=n,c.style(g.elem,a
,h+d);while(n!==(n=g.cur()/q)&&1!==n&&--w)}g.unit=d;g.start=h;g.end=l[1]?h+(l
[1]+1)*e:e}return g}]};c.Animation=c.extend(e,{tweener:function(a,b){c
.isFunction(a)?(b=a,a=["*"]):a=a.split(" ");for(var e,d=0,g=a.length;g>d;d++
)e=a[d],za[e]=za[e]||[],za[e].unshift(b)},prefilter:function(a,
b){b?Ma.unshift(a):Ma.push(a)}});c.Tween=z;z.prototype={constructor:z
,init:function(a,b,e,d,g,l){this.elem=a;this.prop=e;this.easing=g||"swing"
;this.options=b;this.start=this.now=this.cur();this.end=d;this.unit=l||(c
.cssNumber[e]?"":"px")},cur:function(){var a=z.propHooks[this.prop];return a&
&a.get?a.get(this):z.propHooks._default.get(this)},run:function(a){var b,e=z
.propHooks[this.prop];return b=this.options.duration?c.easing[this.easing](a
,this.options.duration*a,0,1,this.options.duration):a,this.now=
(this.end-this.start)*b+this.start,this.options.step&&this.options.step.call
(this.elem,this.now,this),e&&e.set?e.set(this):z.propHooks._default.set(this)
,this}};z.prototype.init.prototype=z.prototype;z.propHooks={_default:
{get:function(a){var b;return null==a.elem[a.prop]||a.elem.style&&null!=a.elem
.style[a.prop]?(b=c.css(a.elem,a.prop,"auto"),b&&"auto"!==b?b:0):a.elem[a.prop
]},set:function(a){c.fx.step[a.prop]?c.fx.step[a.prop](a):a.elem.style&&
(null!=a.elem.style[c.cssProps[a.prop]]||c.cssHooks[a.prop])?
c.style(a.elem,a.prop,a.now+a.unit):a.elem[a.prop]=a.now}}};z.propHooks
.scrollTop=z.propHooks.scrollLeft={set:function(a){a.elem.nodeType&&a.elem
.parentNode&&(a.elem[a.prop]=a.now)}};c.each(["toggle","show","hide"],function
(a,b){var e=c.fn[b];c.fn[b]=function(a,c,d){return null==a||"boolean"==typeof
 a?e.apply(this,arguments):this.animate(J(b,!0),a,c,d)}});c.fn.extend(
{fadeTo:function(a,b,c,e){return this.filter(S).css("opacity",0).show().end()
.animate({opacity:b},a,c,e)},animate:function(a,b,d,q){var g=
c.isEmptyObject(a),l=c.speed(b,d,q),h=function(){var b=e(this,c.extend({},a),l
);h.finish=function(){b.stop(!0)};(g||c._data(this,"finish"))&&b.stop(!0)}
;return h.finish=h,g||!1===l.queue?this.each(h):this.queue(l.queue,h)}
,stop:function(a,b,e){var d=function(a){var b=a.stop;delete a.stop;b(e)}
;return"string"!=typeof a&&(e=b,b=a,a=p),b&&!1!==a&&this.queue(a||"fx",[])
,this.each(function(){var b=!0,f=null!=a&&a+"queueHooks",q=c.timers,h=c._data
(this);if(f)h[f]&&h[f].stop&&d(h[f]);else for(f in h)h[f]&&
h[f].stop&&Ec.test(f)&&d(h[f]);for(f=q.length;f--;)q[f].elem!==this||null!=a&
&q[f].queue!==a||(q[f].anim.stop(e),b=!1,q.splice(f,1));!b&&e||c.dequeue(this
,a)})},finish:function(a){return!1!==a&&(a=a||"fx"),this.each(function(){var b
,e=c._data(this),d=e[a+"queue"];b=e[a+"queueHooks"];var g=c.timers,q=d?d
.length:0;e.finish=!0;c.queue(this,a,[]);b&&b.cur&&b.cur.finish&&b.cur.finish
.call(this);for(b=g.length;b--;)g[b].elem===this&&g[b].queue===a&&(g[b].anim
.stop(!0),g.splice(b,1));for(b=0;q>b;b++)d[b]&&
d[b].finish&&d[b].finish.call(this);delete e.finish})}});c.each({slideDown:J
("show"),slideUp:J("hide"),slideToggle:J("toggle"),fadeIn:{opacity:"show"}
,fadeOut:{opacity:"hide"},fadeToggle:{opacity:"toggle"}},function(a,b){c.fn[a
]=function(a,c,e){return this.animate(b,a,c,e)}});c.speed=function(a,b,e){var
 d=a&&"object"==typeof a?c.extend({},a):{complete:e||!e&&b||c.isFunction(a)&&a
,duration:a,easing:e&&b||b&&!c.isFunction(b)&&b};return d.duration=c.fx
.off?0:"number"==typeof d.duration?d.duration:d.duration in
c.fx.speeds?c.fx.speeds[d.duration]:c.fx.speeds._default,(null==d.queue|
|!0===d.queue)&&(d.queue="fx"),d.old=d.complete,d.complete=function(){c
.isFunction(d.old)&&d.old.call(this);d.queue&&c.dequeue(this,d.queue)},d};c
.easing={linear:function(a){return a},swing:function(a){return.5-Math.cos
(a*Math.PI)/2}};c.timers=[];c.fx=z.prototype.init;c.fx.tick=function(){var a
,b=c.timers,e=0;for(la=c.now();b.length>e;e++)a=b[e],a()||b[e]!==a||b.splice
(e--,1);b.length||c.fx.stop();la=p};c.fx.timer=function(a){a()&&
c.timers.push(a)&&c.fx.start()};c.fx.interval=13;c.fx.start=function(){Ta||
(Ta=setInterval(c.fx.tick,c.fx.interval))};c.fx.stop=function(){clearInterval
(Ta);Ta=null};c.fx.speeds={slow:600,fast:200,_default:400};c.fx.step={};c.expr
&&c.expr.filters&&(c.expr.filters.animated=function(a){return c.grep(c.timers
,function(b){return a===b.elem}).length});c.fn.offset=function(a){if(arguments
.length)return a===p?this:this.each(function(b){c.offset.setOffset(this,a,b)})
;var b,e,d={top:0,left:0},g=this[0],q=
g&&g.ownerDocument;if(q)return b=q.documentElement,c.contains(b,g)?(g
.getBoundingClientRect!==p&&(d=g.getBoundingClientRect()),e=N(q),{top:d.top+(e
.pageYOffset||b.scrollTop)-(b.clientTop||0),left:d.left+(e.pageXOffset||b
.scrollLeft)-(b.clientLeft||0)}):d};c.offset={setOffset:function(a,b,e){var
 d=c.css(a,"position");"static"===d&&(a.style.position="relative");var g,q,h=c
(a),n=h.offset(),w=c.css(a,"top"),m=c.css(a,"left"),d=("absolute"===d|
|"fixed"===d)&&-1<c.inArray("auto",[w,m]),r={},p={};d?(p=h.position(),
g=p.top,q=p.left):(g=parseFloat(w)||0,q=parseFloat(m)||0);c.isFunction(b)&&
(b=b.call(a,e,n));null!=b.top&&(r.top=b.top-n.top+g);null!=b.left&&(r.left=b
.left-n.left+q);"using"in b?b.using.call(a,r):h.css(r)}};c.fn.extend(
{position:function(){if(this[0]){var a,b,e={top:0,left:0},d=this[0]
;return"fixed"===c.css(d,"position")?b=d.getBoundingClientRect():(a=this
.offsetParent(),b=this.offset(),c.nodeName(a[0],"html")||(e=a.offset()),e
.top+=c.css(a[0],"borderTopWidth",!0),e.left+=c.css(a[0],"borderLeftWidth",
!0)),{top:b.top-e.top-c.css(d,"marginTop",!0),left:b.left-e.left-c.css(d
,"marginLeft",!0)}}},offsetParent:function(){return this.map(function(){for
(var a=this.offsetParent||y.documentElement;a&&!c.nodeName(a,"html")&
&"static"===c.css(a,"position");)a=a.offsetParent;return a||y.documentElement}
)}});c.each({scrollLeft:"pageXOffset",scrollTop:"pageYOffset"},function(a,b)
{var e=/Y/.test(b);c.fn[a]=function(d){return c.access(this,function(a,d,q)
{var h=N(a);return q===p?h?b in h?h[b]:h.document.documentElement[d]:
a[d]:(h?h.scrollTo(e?c(h).scrollLeft():q,e?q:c(h).scrollTop()):a[d]=q,p)},a,d
,arguments.length,null)}});c.each({Height:"height",Width:"width"},function(a,b
){c.each({padding:"inner"+a,content:b,"":"outer"+a},function(e,d){c.fn[d
]=function(g,d){var q=arguments.length&&(e||"boolean"!=typeof g),h=e||(!0===g|
|!0===d?"margin":"border");return c.access(this,function(b,e,f){var g;return c
.isWindow(b)?b.document.documentElement["client"+a]:9===b.nodeType?(g=b
.documentElement,Math.max(b.body["scroll"+a],g["scroll"+
a],b.body["offset"+a],g["offset"+a],g["client"+a])):f===p?c.css(b,e,h):c.style
(b,e,f,h)},b,q?g:p,q,null)}})});v.jQuery=v.$=c;"function"==typeof define&
&define.amd&&define.amd.jQuery&&define("jquery",[],function(){return c})})
(window);function updateUI(v){document.body.style.opacity=0;(v="value"in v&&v
.value)?(document.getElementById("proxy_on").style.display="block",document
.getElementById("proxy_off").style.display="none",chrome.browserAction.setIcon
({path:{19:"./images/proxy-enabled19.webp",38:"./images/proxy-enabled38.webp"}
}),chrome.preferencesPrivate.dataReductionUpdateDailyLengths.onChange
.addListener(onSavingsDataChanged),chrome.preferencesPrivate
.dataReductionUpdateDailyLengths.set({value:!0})):(document.getElementById
("proxy_off").style.display=
"block",document.getElementById("proxy_on").style.display="none",chrome
.browserAction.setIcon({path:{19:"./images/proxy-disabled19.png",38:"
./images/proxy-disabled38.png"}}),chrome.preferencesPrivate
.dataReductionUpdateDailyLengths.onChange.removeListener(onSavingsDataChanged)
);$("body").fadeTo(400,1)}
function onSavingsDataChanged(v){var p=null,m=null;"value"in v&&!v.value&&
(chrome.dataReductionProxy.dataReductionDailyContentLength.get({},function(d)
{"value"in d&&(p=d.value);drawDataSavingsChart(p,m)}),chrome
.dataReductionProxy.dataReductionDailyReceivedLength.get({},function(d)
{"value"in d&&(m=d.value);drawDataSavingsChart(p,m)}))}var
 isGraphAnimationInProgress=!1,chart=null;
function drawDataSavingsChart(v,p){if(v&&p&&!isGraphAnimationInProgress)
{isGraphAnimationInProgress=!0;var m=Array(30),d=v.length-30;v.splice(0,d);p
.splice(0,d);for(var d=Array(30),n=Array(30),h=0,r=0,B=0;30>B;B++){m[B]="";var
 D=v[B]?parseInt(v[B],10):0,M=p[B]?parseInt(p[B],10):0;if(0>D||0>M)M=D=0;d[B
]=h+D;n[B]=r+M;h=d[B];r=n[B]}r=d[29];h=n[29];B=0==r?0:100*(r-h)/r;0>B&&(B=0
,n=d,h=r);B=B.toFixed(1)+"";document.getElementById("data_savings_percent")
.innerText=chrome.i18n.getMessage("dataSavingsPercentFormat",
B);B=chrome.i18n.getMessage("originalSizeFormat",""+r);D=chrome.i18n
.getMessage("compressedSizeFormat",""+h);1073741824<h?(r=(r/1073741824)
.toFixed(1)+"",h=(h/1073741824).toFixed(1)+"",B=chrome.i18n.getMessage
("originalSizeFormatGb",r),D=chrome.i18n.getMessage("compressedSizeFormatGb",h
)):1048576<h?(r=(r/1048576).toFixed(1)+"",h=(h/1048576).toFixed(1)+"",B=chrome
.i18n.getMessage("originalSizeFormatMb",r),D=chrome.i18n.getMessage
("compressedSizeFormatMb",h)):1024<h&&(r=(r/1024).toFixed(1)+"",h=(h/
1024).toFixed(1)+"",B=chrome.i18n.getMessage("originalSizeFormatKb",r)
,D=chrome.i18n.getMessage("compressedSizeFormatKb",h));document.getElementById
("original_data_size").innerHTML=B;document.getElementById
("compressed_data_size").innerHTML=D;m={labels:m,datasets:[{fillColor:"rgba
(217, 217, 217, 1)",strokeColor:"rgba(217 , 217, 217, 1)",data:d},
{fillColor:"rgba(3, 169, 244, 1)",strokeColor:"rgba(0, 0, 0, 0)",data:n}]};d=
{bezierCurveTension:.1,animationSteps:10,animationEasing:"easeInOutSine"
,datasetStrokeWidth:1,
pointDot:!1,scaleShowGridLines:!1,showScale:!1,scaleBeginAtZero:!0
,showTooltips:!1,onAnimationComplete:function(){isGraphAnimationInProgress=!1}
};null==chart&&(n=document.getElementById("data_savings_graph").getContext
("2d"),chart=new Chart(n));chart.Line(m,d)}}function onEnableProxyClicked()
{chrome.dataReductionProxy.spdyProxyEnabled.set({value:!0})}function
 onDisableProxyClicked(){chrome.dataReductionProxy.spdyProxyEnabled.set(
{value:!1})}
document.addEventListener("DOMContentLoaded",function(){createUI()
;"undefined"===typeof chrome.dataReductionProxy||"undefined"===typeof chrome
.preferencesPrivate?document.getElementById("chrome_incompatible").style
.display="block":chrome.windows.getCurrent({},function(v){v.incognito?document
.getElementById("incognito").style.display="block":(document.getElementById
("main").style.display="block",chrome.dataReductionProxy.spdyProxyEnabled.get(
{},updateUI),chrome.dataReductionProxy.spdyProxyEnabled.onChange.addListener
(updateUI))});
/mac/i.test(navigator.platform)&&setTimeout(function(){document.body.style
.marginBottom="9px"},500)});
function createUI(){document.getElementById("ext_name").innerText=chrome.i18n
.getMessage("extNameBeta");document.getElementById
("chrome_incompatible_message").innerText=chrome.i18n.getMessage
("versionNotCompatible");document.getElementById("incognito_message")
.innerText=chrome.i18n.getMessage("incognitoMessage");document.getElementById
("info1").innerText=chrome.i18n.getMessage("info1");document.getElementById
("info2").innerText=chrome.i18n.getMessage("info2");document.getElementById
("learn_more").innerText=
chrome.i18n.getMessage("learnMoreLinkText");document.getElementById
("help_feedback").innerText=chrome.i18n.getMessage("helpAndFeedback");var
 v=document.getElementById("enable_proxy");v.innerText=chrome.i18n.getMessage
("enableDataSaverLabel");v.onclick=onEnableProxyClicked;v=document
.getElementById("disable_proxy");v.innerText=chrome.i18n.getMessage
("disableDataSaverLabel");v.onclick=onDisableProxyClicked;var v=navigator
.language,p=new Date,m=new Date(p.getTime()-2592E6),d={day:"numeric"
,month:"long"};
document.getElementById("graph_start_date").innerText=m.toLocaleDateString(v,d
);document.getElementById("graph_end_date").innerText=p.toLocaleDateString(v,d
)};/*

 Copyright (c) 2013-2014 Nick Downie
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

 Chart.js
 http://chartjs.org/
 Version: 1.0.1-beta.4

 Copyright 2014 Nick Downie
 Released under the MIT license
 https://github.com/nnnick/Chart.js/blob/master/LICENSE.md
*/
(function(){var v=this,p=v.Chart,m=function(b){this.canvas=b.canvas;this.ctx=b
;this.width=b.canvas.width;this.height=b.canvas.height;return this
.aspectRatio=this.width/this.height,d.retinaScale(this),this};m.defaults=
{global:{animation:!0,animationSteps:60,animationEasing:"easeOutQuart"
,showScale:!0,scaleOverride:!1,scaleSteps:null,scaleStepWidth:null
,scaleStartValue:null,scaleLineColor:"rgba(0,0,0,.1)",scaleLineWidth:1
,scaleShowLabels:!0,scaleLabel:"<%=value%>",scaleIntegersOnly:!0
,scaleBeginAtZero:!1,
scaleFontFamily:"'Helvetica Neue', 'Helvetica', 'Arial', sans-serif"
,scaleFontSize:12,scaleFontStyle:"normal",scaleFontColor:"#666",responsive:!1
,maintainAspectRatio:!0,showTooltips:!0,tooltipEvents:["mousemove"
,"touchstart","touchmove","mouseout"],tooltipFillColor:"rgba(0,0,0,0.8)"
,tooltipFontFamily:"'Helvetica Neue', 'Helvetica', 'Arial', sans-serif"
,tooltipFontSize:14,tooltipFontStyle:"normal",tooltipFontColor:"#fff"
,tooltipTitleFontFamily:"'Helvetica Neue', 'Helvetica', 'Arial', sans-serif"
,tooltipTitleFontSize:14,
tooltipTitleFontStyle:"bold",tooltipTitleFontColor:"#fff",tooltipYPadding:6
,tooltipXPadding:6,tooltipCaretSize:8,tooltipCornerRadius:6,tooltipXOffset:10
,tooltipTemplate:"<%if (label){%><%=label%>: <%}%><%= value %>"
,multiTooltipTemplate:"<%= value %>",multiTooltipKeyBackground:"#fff"
,onAnimationProgress:function(){},onAnimationComplete:function(){}}};m.types={
};var d=m.helpers={},n=d.each=function(b,e,d){var h=Array.prototype.slice.call
(arguments,3);if(b)if(b.length===+b.length){var n;for(n=0;n<b.length;n++)e
.apply(d,
[b[n],n].concat(h))}else for(n in b)e.apply(d,[b[n],n].concat(h))},h=d
.clone=function(b){var e={};return n(b,function(d,h){b.hasOwnProperty(h)&&(e[h
]=d)}),e},r=d.extend=function(b){return n(Array.prototype.slice.call(arguments
,1),function(e){n(e,function(d,h){e.hasOwnProperty(h)&&(b[h]=d)})}),b},B=d
.merge=function(){var b=Array.prototype.slice.call(arguments,0);return b
.unshift({}),r.apply(null,b)},D=d.indexOf=function(b,e){if(Array.prototype
.indexOf)return b.indexOf(e);for(var d=0;d<b.length;d++)if(b[d]===
e)return d;return-1},M=(d.where=function(b,e){var q=[];return d.each(b
,function(b){e(b)&&q.push(b)}),q},d.findNextWhere=function(b,e,d){d||(d=-1)
;for(d+=1;d<b.length;d++){var h=b[d];if(e(h))return h}},d
.findPreviousWhere=function(b,e,d){d||(d=b.length);for(--d;0<=d;d--){var h=b[d
];if(e(h))return h}},d.inherits=function(b){var e=this,d=b&&b.hasOwnProperty
("constructor")?b.constructor:function(){return e.apply(this,arguments)}
,h=function(){this.constructor=d};return h.prototype=e.prototype,d.prototype=
new h,d.extend=M,b&&r(d.prototype,b),d.__super__=e.prototype,d}),E=d
.noop=function(){},A=d.uid=function(){var b=0;return function()
{return"chart-"+b++}}(),V=d.warn=function(b){window.console&
&"function"==typeof window.console.warn&&console.warn(b)},ra=d
.amd="function"==typeof v.define&&v.define.amd,Q=d.isNumber=function(b)
{return!isNaN(parseFloat(b))&&isFinite(b)},ea=d.max=function(b){return Math
.max.apply(Math,b)},R=d.min=function(b){return Math.min.apply(Math,b)},sa=(d
.cap=function(b,e,d){if(Q(e)){if(b>
e)return e}else if(Q(d)&&d>b)return d;return b},d.getDecimalPlaces=function(b)
{return 0!==b%1&&Q(b)?b.toString().split(".")[1].length:0}),F=d
.radians=function(b){return Math.PI/180*b},Da=(d.getAngleFromPoint=function(b
,e){var d=e.x-b.x,h=e.y-b.y,n=Math.sqrt(d*d+h*h),m=2*Math.PI+Math.atan2(h,d)
;return 0>d&&0>h&&(m+=2*Math.PI),{angle:m,distance:n}},d.aliasPixel=function(b
){return 0===b%2?0:.5}),Ea=(d.splineCurve=function(b,e,d,h){var n=Math.sqrt
(Math.pow(e.x-b.x,2)+Math.pow(e.y-b.y,2)),m=Math.sqrt(Math.pow(d.x-
e.x,2)+Math.pow(d.y-e.y,2)),r=h*n/(n+m);h=h*m/(n+m);return{inner:{x:e.x-r*(d
.x-b.x),y:e.y-r*(d.y-b.y)},outer:{x:e.x+h*(d.x-b.x),y:e.y+h*(d.y-b.y)}}},d
.calculateOrderOfMagnitude=function(b){return Math.floor(Math.log(b)/Math.LN10
)}),S=(d.calculateScaleRange=function(b,e,d,h,n){e=Math.floor(e/(1.5*d))
;d=2>=e;var m=ea(b),r=R(b);m===r&&(m+=.5,.5<=r&&!h?r-=.5:m+=.5);b=Math.abs(m-r
);b=Ea(b);m=Math.ceil(m/(1*Math.pow(10,b)))*Math.pow(10,b);h=h?0:Math.floor(r/
(1*Math.pow(10,b)))*Math.pow(10,b);for(var m=
m-h,r=Math.pow(10,b),p=Math.round(m/r);(p>e||e>2*p)&&!d;)if(p>e)r*=2,p=Math
.round(m/r),0!==p%1&&(d=!0);else{if(n&&0<=b&&0!==r/2%1)break;r/=2;p=Math.round
(m/r)}return d&&(p=2,r=m/p),{steps:p,stepValue:r,min:h,max:h+p*r}},d
.template=function(b,e){if(b instanceof Function)return b(e);var d={},h,n=b
;h=e;d=/\W/.test(n)?new Function("obj","var p=[],print=function(){p.push.apply
(p,arguments);};with(obj){p.push('"+n.replace(/[\r\t\n]/g," ").split("<%")
.join("\t").replace(/((^|%>)[^\t]*)'/g,"$1\r").replace(/\t=(.*?)%>/g,
"',$1,'").split("\t").join("');").split("%>").join("p.push('").split("\r")
.join("\\'")+"');}return p.join('');"):d[n]=d[n];return h=h?d(h):d}),X=(d
.generateLabels=function(b,e,d,h){var m=Array(e);return labelTemplateString&&n
(m,function(e,n){m[n]=S(b,{value:d+h*(n+1)})}),m},d.easingEffects=
{linear:function(b){return b},easeInQuad:function(b){return b*b}
,easeOutQuad:function(b){return-1*b*(b-2)},easeInOutQuad:function(b){return 1>
(b/=.5)?.5*b*b:-.5*(--b*(b-2)-1)},easeInCubic:function(b){return b*b*
b},easeOutCubic:function(b){return 1*((b=b/1-1)*b*b+1)}
,easeInOutCubic:function(b){return 1>(b/=.5)?.5*b*b*b:.5*((b-=2)*b*b+2)}
,easeInQuart:function(b){return b*b*b*b},easeOutQuart:function(b){return-1*(
(b=b/1-1)*b*b*b-1)},easeInOutQuart:function(b){return 1>(b/=.5)?.5*b*b*b*b:-
.5*((b-=2)*b*b*b-2)},easeInQuint:function(b){return 1*(b/=1)*b*b*b*b}
,easeOutQuint:function(b){return 1*((b=b/1-1)*b*b*b*b+1)}
,easeInOutQuint:function(b){return 1>(b/=.5)?.5*b*b*b*b*b:.5*((b-=2)*b*b*b*b+2
)},easeInSine:function(b){return-1*
Math.cos(b/1*(Math.PI/2))+1},easeOutSine:function(b){return 1*Math.sin(b/1*
(Math.PI/2))},easeInOutSine:function(b){return-.5*(Math.cos(Math.PI*b/1)-1)}
,easeInExpo:function(b){return 0===b?1:1*Math.pow(2,10*(b/1-1))}
,easeOutExpo:function(b){return 1===b?1:1*(-Math.pow(2,-10*b/1)+1)}
,easeInOutExpo:function(b){return 0===b?0:1===b?1:1>(b/=.5)?.5*Math.pow(2,10*
(b-1)):.5*(-Math.pow(2,-10*--b)+2)},easeInCirc:function(b){return 1<=b?b:-1*
(Math.sqrt(1-(b/=1)*b)-1)},easeOutCirc:function(b){return 1*Math.sqrt(1-
(b=b/1-1)*b)},easeInOutCirc:function(b){return 1>(b/=.5)?-.5*(Math.sqrt(1-b*b
)-1):.5*(Math.sqrt(1-(b-=2)*b)+1)},easeInElastic:function(b){var e=1.70158,d=0
,h=1;return 0===b?0:1==(b/=1)?1:(d||(d=.3),h<Math.abs(1)?(h=1,e=d/4):e=d/
(2*Math.PI)*Math.asin(1/h),-(h*Math.pow(2,10*--b)*Math.sin(2*(1*b-e)*Math.PI/d
)))},easeOutElastic:function(b){var e=1.70158,d=0,h=1;return 0===b?0:1==(b/=1
)?1:(d||(d=.3),h<Math.abs(1)?(h=1,e=d/4):e=d/(2*Math.PI)*Math.asin(1/h),h*Math
.pow(2,-10*b)*Math.sin(2*(1*b-e)*Math.PI/
d)+1)},easeInOutElastic:function(b){var e=1.70158,d=0,h=1;return 0===b?0:2==
(b/=.5)?1:(d||(d=.3*1.5),h<Math.abs(1)?(h=1,e=d/4):e=d/(2*Math.PI)*Math.asin
(1/h),1>b?-.5*h*Math.pow(2,10*--b)*Math.sin(2*(1*b-e)*Math.PI/d):h*Math.pow(2
,-10*--b)*Math.sin(2*(1*b-e)*Math.PI/d)*.5+1)},easeInBack:function(b){return
 1*(b/=1)*b*(2.70158*b-1.70158)},easeOutBack:function(b){return 1*((b=b/1-1
)*b*(2.70158*b+1.70158)+1)},easeInOutBack:function(b){var e=1.70158;return 1>
(b/=.5)?.5*b*b*(((e*=1.525)+1)*b-e):.5*((b-=
2)*b*(((e*=1.525)+1)*b+e)+2)},easeInBounce:function(b){return 1-X
.easeOutBounce(1-b)},easeOutBounce:function(b){return(b/=1)<1/2.75?7
.5625*b*b:2/2.75>b?1*(7.5625*(b-=1.5/2.75)*b+.75):2.5/2.75>b?1*(7.5625*(b-=2
.25/2.75)*b+.9375):1*(7.5625*(b-=2.625/2.75)*b+.984375)}
,easeInOutBounce:function(b){return.5>b?.5*X.easeInBounce(2*b):.5*X
.easeOutBounce(2*b-1)+.5}}),va=d.requestAnimFrame=function(){return window
.requestAnimationFrame||window.webkitRequestAnimationFrame||window
.mozRequestAnimationFrame||window.oRequestAnimationFrame||
window.msRequestAnimationFrame||function(b){return window.setTimeout(b,1E3/60)
}}(),Ga=(d.cancelAnimFrame=function(){return window.cancelAnimationFrame|
|window.webkitCancelAnimationFrame||window.mozCancelAnimationFrame||window
.oCancelAnimationFrame||window.msCancelAnimationFrame||function(b){return
 window.clearTimeout(b,1E3/60)}}(),d.animationLoop=function(b,e,d,h,n,m){var
 r=0,p=X[d]||X.linear,v=function(){r++;var d=r/e,q=p(d);b.call(m,q,d,r);h.call
(m,q,d);e>r?m.animationFrame=va(v):n.apply(m)};va(v)},
d.getRelativePosition=function(b){var e,d,h=b.originalEvent||b;b=b
.currentTarget||b.srcElement;b=b.getBoundingClientRect();return h.touches?(e=h
.touches[0].clientX-b.left,d=h.touches[0].clientY-b.top):(e=h.clientX-b.left
,d=h.clientY-b.top),{x:e,y:d}},d.addEvent=function(b,e,d){b.addEventListener?b
.addEventListener(e,d):b.attachEvent?b.attachEvent("on"+e,d):b["on"+e]=d})
,Ha=d.removeEvent=function(b,e,d){b.removeEventListener?b.removeEventListener
(e,d,!1):b.detachEvent?b.detachEvent("on"+e,d):b["on"+
e]=E},Fa=(d.bindEvents=function(b,e,d){b.events||(b.events={});n(e,function(e)
{b.events[e]=function(){d.apply(b,arguments)};Ga(b.chart.canvas,e,b.events[e])
})},d.unbindEvents=function(b,e){n(e,function(e,d){Ha(b.chart.canvas,d,e)})})
,Ja=d.getMaximumWidth=function(b){b=b.parentNode;return b.clientWidth},xa=d
.getMaximumHeight=function(b){b=b.parentNode;return b.clientHeight},Ka=(d
.getMaximumSize=d.getMaximumWidth,d.retinaScale=function(b){var e=b.ctx,d=b
.canvas.width;b=b.canvas.height;window.devicePixelRatio&&
(e.canvas.style.width=d+"px",e.canvas.style.height=b+"px",e.canvas
.height=b*window.devicePixelRatio,e.canvas.width=d*window.devicePixelRatio,e
.scale(window.devicePixelRatio,window.devicePixelRatio))}),La=d.clear=function
(b){b.ctx.clearRect(0,0,b.width,b.height)},P=d.fontString=function(b,e,d)
{return e+" "+b+"px "+d},ka=d.longestText=function(b,e,d){b.font=e;var h=0
;return n(d,function(e){e=b.measureText(e).width;h=e>h?e:h}),h},ya=d
.drawRoundedRectangle=function(b,e,d,h,n,m){b.beginPath();b.moveTo(e+
m,d);b.lineTo(e+h-m,d);b.quadraticCurveTo(e+h,d,e+h,d+m);b.lineTo(e+h,d+n-m);b
.quadraticCurveTo(e+h,d+n,e+h-m,d+n);b.lineTo(e+m,d+n);b.quadraticCurveTo(e
,d+n,e,d+n-m);b.lineTo(e,d+m);b.quadraticCurveTo(e,d,e+m,d);b.closePath()};m
.instances={};m.Type=function(b,e,d){this.options=e;this.chart=d;this.id=A();m
.instances[this.id]=this;e.responsive&&this.resize();this.initialize.call(this
,b)};r(m.Type.prototype,{initialize:function(){return this},clear:function()
{return La(this.chart),this},stop:function(){return d.cancelAnimFrame.call(v,
this.animationFrame),this},resize:function(b){this.stop();var e=this.chart
.canvas,d=Ja(this.chart.canvas),h=this.options.maintainAspectRatio?d/this
.chart.aspectRatio:xa(this.chart.canvas);return e.width=this.chart.width=d,e
.height=this.chart.height=h,Ka(this.chart),"function"==typeof b&&b.apply(this
,Array.prototype.slice.call(arguments,1)),this},reflow:E,render:function(b)
{return b&&this.reflow(),this.options.animation&&!b?d.animationLoop(this.draw
,this.options.animationSteps,this.options.animationEasing,
this.options.onAnimationProgress,this.options.onAnimationComplete,this):(this
.draw(),this.options.onAnimationComplete.call(this)),this}
,generateLegend:function(){return S(this.options.legendTemplate,this)}
,destroy:function(){this.clear();Fa(this,this.events);delete m.instances[this
.id]},showTooltip:function(b,e){"undefined"==typeof this.activeElements&&(this
.activeElements=[]);var h=function(b){var e=!1;return b.length!==this
.activeElements.length?e=!0:(n(b,function(b,d){b!==this.activeElements[d]&&
(e=!0)},this),e)}.call(this,b);if(h||e){if(this.activeElements=b,this.draw()
,0<b.length)if(this.datasets&&1<this.datasets.length){for(var r,p,h=this
.datasets.length-1;0<=h&&(r=this.datasets[h].points||this.datasets[h].bars|
|this.datasets[h].segments,p=D(r,b[0]),-1===p);h--);var v=[],B=[];r=function()
{var b,e,h,n,q,m=[],r=[],w=[];return d.each(this.datasets,function(e){b=e
.points||e.bars||e.segments;b[p]&&b[p].hasValue()&&m.push(b[p])}),d.each(m
,function(b){r.push(b.x);w.push(b.y);v.push(d.template(this.options
.multiTooltipTemplate,
b));B.push({fill:b._saved.fillColor||b.fillColor,stroke:b._saved.strokeColor|
|b.strokeColor})},this),q=R(w),h=ea(w),n=R(r),e=ea(r),{x:n>this.chart
.width/2?n:e,y:(q+h)/2}}.call(this,p);(new m.MultiTooltip({x:r.x,y:r.y
,xPadding:this.options.tooltipXPadding,yPadding:this.options.tooltipYPadding
,xOffset:this.options.tooltipXOffset,fillColor:this.options.tooltipFillColor
,textColor:this.options.tooltipFontColor,fontFamily:this.options
.tooltipFontFamily,fontStyle:this.options.tooltipFontStyle,fontSize:this
.options.tooltipFontSize,
titleTextColor:this.options.tooltipTitleFontColor,titleFontFamily:this.options
.tooltipTitleFontFamily,titleFontStyle:this.options.tooltipTitleFontStyle
,titleFontSize:this.options.tooltipTitleFontSize,cornerRadius:this.options
.tooltipCornerRadius,labels:v,legendColors:B,legendColorBackground:this
.options.multiTooltipKeyBackground,title:b[0].label,chart:this.chart,ctx:this
.chart.ctx})).draw()}else n(b,function(b){var e=b.tooltipPosition();(new m
.Tooltip({x:Math.round(e.x),y:Math.round(e.y),xPadding:this.options
.tooltipXPadding,
yPadding:this.options.tooltipYPadding,fillColor:this.options.tooltipFillColor
,textColor:this.options.tooltipFontColor,fontFamily:this.options
.tooltipFontFamily,fontStyle:this.options.tooltipFontStyle,fontSize:this
.options.tooltipFontSize,caretHeight:this.options.tooltipCaretSize
,cornerRadius:this.options.tooltipCornerRadius,text:S(this.options
.tooltipTemplate,b),chart:this.chart})).draw()},this);return this}}
,toBase64Image:function(){return this.chart.canvas.toDataURL.apply(this.chart
.canvas,arguments)}});
m.Type.extend=function(b){var e=this,d=function(){return e.apply(this
,arguments)};if(d.prototype=h(e.prototype),r(d.prototype,b),d.extend=m.Type
.extend,b.name||e.prototype.name){var n=b.name||e.prototype.name,p=m.defaults
[e.prototype.name]?h(m.defaults[e.prototype.name]):{};m.defaults[n]=r(p,b
.defaults);m.types[n]=d;m.prototype[n]=function(b,e){var h=B(m.defaults.global
,m.defaults[n],e||{});return new d(b,h,this)}}else V("Name not provided for
 this chart, so it hasn't been registered");return e};
m.Element=function(b){r(this,b);this.initialize.apply(this,arguments);this
.save()};r(m.Element.prototype,{initialize:function(){},restore:function(b)
{return b?n(b,function(b){this[b]=this._saved[b]},this):r(this,this._saved)
,this},save:function(){return this._saved=h(this),delete this._saved._saved
,this},update:function(b){return n(b,function(b,d){this._saved[d]=this[d];this
[d]=b},this),this},transition:function(b,e){return n(b,function(b,d){this[d]=
(b-this._saved[d])*e+this._saved[d]},this),this},
tooltipPosition:function(){return{x:this.x,y:this.y}},hasValue:function()
{return Q(this.value)}});m.Element.extend=M;m.Point=m.Element.extend(
{display:!0,inRange:function(b,e){var d=this.hitDetectionRadius+this.radius
;return Math.pow(b-this.x,2)+Math.pow(e-this.y,2)<Math.pow(d,2)},draw:function
(){if(this.display){var b=this.ctx;b.beginPath();b.arc(this.x,this.y,this
.radius,0,2*Math.PI);b.closePath();b.strokeStyle=this.strokeColor;b
.lineWidth=this.strokeWidth;b.fillStyle=this.fillColor;b.fill();b.stroke()}}})
;
m.Arc=m.Element.extend({inRange:function(b,e){var h=d.getAngleFromPoint(this,
{x:b,y:e}),n=h.angle>=this.startAngle&&h.angle<=this.endAngle,h=h
.distance>=this.innerRadius&&h.distance<=this.outerRadius;return n&&h}
,tooltipPosition:function(){var b=this.startAngle+(this.endAngle-this
.startAngle)/2,e=(this.outerRadius-this.innerRadius)/2+this.innerRadius;return
{x:this.x+Math.cos(b)*e,y:this.y+Math.sin(b)*e}},draw:function(){var b=this
.ctx;b.beginPath();b.arc(this.x,this.y,this.outerRadius,this.startAngle,
this.endAngle);b.arc(this.x,this.y,this.innerRadius,this.endAngle,this
.startAngle,!0);b.closePath();b.strokeStyle=this.strokeColor;b.lineWidth=this
.strokeWidth;b.fillStyle=this.fillColor;b.fill();b.lineJoin="bevel";this
.showStroke&&b.stroke()}});m.Rectangle=m.Element.extend({draw:function(){var
 b=this.ctx,e=this.width/2,d=this.x-e,e=this.x+e,h=this.base-(this.base-this.y
),n=this.strokeWidth/2;this.showStroke&&(d+=n,e-=n,h+=n);b.beginPath();b
.fillStyle=this.fillColor;b.strokeStyle=this.strokeColor;
b.lineWidth=this.strokeWidth;b.moveTo(d,this.base);b.lineTo(d,h);b.lineTo(e,h)
;b.lineTo(e,this.base);b.fill();this.showStroke&&b.stroke()},height:function()
{return this.base-this.y},inRange:function(b,e){return b>=this.x-this.width/2&
&b<=this.x+this.width/2&&e>=this.y&&e<=this.base}});m.Tooltip=m.Element.extend
({draw:function(){var b=this.chart.ctx;b.font=P(this.fontSize,this.fontStyle
,this.fontFamily);this.xAlign="center";this.yAlign="above";var e=b.measureText
(this.text).width+2*this.xPadding,d=
this.fontSize+2*this.yPadding,h=d+this.caretHeight+2;this.x+e/2>this.chart
.width?this.xAlign="left":0>this.x-e/2&&(this.xAlign="right");0>this.y-h&&
(this.yAlign="below");var n=this.x-e/2,h=this.y-h;switch(b.fillStyle=this
.fillColor,this.yAlign){case "above":b.beginPath();b.moveTo(this.x,this.y-2);b
.lineTo(this.x+this.caretHeight,this.y-(2+this.caretHeight));b.lineTo(this
.x-this.caretHeight,this.y-(2+this.caretHeight));b.closePath();b.fill();break
;case "below":h=this.y+2+this.caretHeight,b.beginPath(),
b.moveTo(this.x,this.y+2),b.lineTo(this.x+this.caretHeight,this.y+2+this
.caretHeight),b.lineTo(this.x-this.caretHeight,this.y+2+this.caretHeight),b
.closePath(),b.fill()}switch(this.xAlign){case "left":n=this.x-e+(this
.cornerRadius+this.caretHeight);break;case "right":n=this.x-(this
.cornerRadius+this.caretHeight)}ya(b,n,h,e,d,this.cornerRadius);b.fill();b
.fillStyle=this.textColor;b.textAlign="center";b.textBaseline="middle";b
.fillText(this.text,n+e/2,h+d/2)}});m.MultiTooltip=m.Element.extend(
{initialize:function(){this.font=
P(this.fontSize,this.fontStyle,this.fontFamily);this.titleFont=P(this
.titleFontSize,this.titleFontStyle,this.titleFontFamily);this.height=this
.labels.length*this.fontSize+this.fontSize/2*(this.labels.length-1)+2*this
.yPadding+1.5*this.titleFontSize;this.ctx.font=this.titleFont;var b=this.ctx
.measureText(this.title).width,e=ka(this.ctx,this.font,this.labels)+this
.fontSize+3,b=ea([e,b]);this.width=b+2*this.xPadding;b=this.height/2;0>this
.y-b?this.y=b:this.y+b>this.chart.height&&(this.y=this.chart.height-
b);this.x>this.chart.width/2?this.x-=this.xOffset+this.width:this.x+=this
.xOffset},getLineHeight:function(b){var e=this.y-this.height/2+this.yPadding
,d=b-1;return 0===b?e+this.titleFontSize/2:e+(1.5*this.fontSize*d+this
.fontSize/2)+1.5*this.titleFontSize},draw:function(){ya(this.ctx,this.x,this
.y-this.height/2,this.width,this.height,this.cornerRadius);var b=this.ctx;b
.fillStyle=this.fillColor;b.fill();b.closePath();b.textAlign="left";b
.textBaseline="middle";b.fillStyle=this.titleTextColor;b.font=
this.titleFont;b.fillText(this.title,this.x+this.xPadding,this.getLineHeight(0
));b.font=this.font;d.each(this.labels,function(e,d){b.fillStyle=this
.textColor;b.fillText(e,this.x+this.xPadding+this.fontSize+3,this
.getLineHeight(d+1));b.fillStyle=this.legendColorBackground;b.fillRect(this
.x+this.xPadding,this.getLineHeight(d+1)-this.fontSize/2,this.fontSize,this
.fontSize);b.fillStyle=this.legendColors[d].fill;b.fillRect(this.x+this
.xPadding,this.getLineHeight(d+1)-this.fontSize/2,this.fontSize,this.fontSize)
},
this)}});m.Scale=m.Element.extend({initialize:function(){this.fit()}
,buildYLabels:function(){this.yLabels=[];for(var b=sa(this.stepValue),e=0
;e<=this.steps;e++)this.yLabels.push(S(this.templateString,{value:(this
.min+e*this.stepValue).toFixed(b)}));this.yLabelWidth=this.display&&this
.showLabels?ka(this.ctx,this.font,this.yLabels):0},addXLabel:function(b){this
.xLabels.push(b);this.valuesCount++;this.fit()},removeXLabel:function(){this
.xLabels.shift();this.valuesCount--;this.fit()},fit:function(){this
.startPoint=
this.display?this.fontSize:0;this.endPoint=this.display?this.height-1.5*this
.fontSize-5:this.height;this.startPoint+=this.padding;this.endPoint-=this
.padding;var b,e=this.endPoint-this.startPoint;this.calculateYRange(e);this
.buildYLabels();for(this.calculateXLabelRotation();e>this.endPoint-this
.startPoint;)e=this.endPoint-this.startPoint,b=this.yLabelWidth,this
.calculateYRange(e),this.buildYLabels(),b<this.yLabelWidth&&this
.calculateXLabelRotation()},calculateXLabelRotation:function(){this.ctx.font=
this.font;var b,e=this.ctx.measureText(this.xLabels[0]).width;b=this.ctx
.measureText(this.xLabels[this.xLabels.length-1]).width;if(this
.xScalePaddingRight=b/2+3,this.xScalePaddingLeft=e/2>this
.yLabelWidth+10?e/2:this.yLabelWidth+10,this.xLabelRotation=0,this.display)
{var d,h=ka(this.ctx,this.font,this.xLabels);this.xLabelWidth=h;for(var n=Math
.floor(this.calculateX(1)-this.calculateX(0))-6;this.xLabelWidth>n&&0===this
.xLabelRotation||this.xLabelWidth>n&&90>=this.xLabelRotation&&0<this
.xLabelRotation;)d=
Math.cos(F(this.xLabelRotation)),b=d*e,b+this.fontSize/2>this.yLabelWidth+8&&
(this.xScalePaddingLeft=b+this.fontSize/2),this.xScalePaddingRight=this
.fontSize/2,this.xLabelRotation++,this.xLabelWidth=d*h;0<this.xLabelRotation&&
(this.endPoint-=Math.sin(F(this.xLabelRotation))*h+3)}else this.xLabelWidth=0
,this.xScalePaddingLeft=this.xScalePaddingRight=this.padding}
,calculateYRange:E,drawingArea:function(){return this.startPoint-this.endPoint
},calculateY:function(b){var e=this.drawingArea()/(this.min-
this.max);return this.endPoint-e*(b-this.min)},calculateX:function(b){var e=
(0<this.xLabelRotation,this.width-(this.xScalePaddingLeft+this
.xScalePaddingRight)),e=e/(this.valuesCount-(this.offsetGridLines?0:1))
;b=e*b+this.xScalePaddingLeft;return this.offsetGridLines&&(b+=e/2),Math.round
(b)},update:function(b){d.extend(this,b);this.fit()},draw:function(){var
 b=this.ctx,e=(this.endPoint-this.startPoint)/this.steps,h=Math.round(this
.xScalePaddingLeft);this.display&&(b.fillStyle=this.textColor,b.font=
this.font,n(this.yLabels,function(n,m){var r=this.endPoint-e*m,p=Math.round(r)
;b.textAlign="right";b.textBaseline="middle";this.showLabels&&b.fillText(n
,h-10,r);b.beginPath();0<m?(b.lineWidth=this.gridLineWidth,b.strokeStyle=this
.gridLineColor):(b.lineWidth=this.lineWidth,b.strokeStyle=this.lineColor);p+=d
.aliasPixel(b.lineWidth);b.moveTo(h,p);b.lineTo(this.width,p);b.stroke();b
.closePath();b.lineWidth=this.lineWidth;b.strokeStyle=this.lineColor;b
.beginPath();b.moveTo(h-5,p);b.lineTo(h,p);b.stroke();
b.closePath()},this),n(this.xLabels,function(e,d){var h=this.calculateX(d)+Da
(this.lineWidth),n=this.calculateX(d-(this.offsetGridLines?.5:0))+Da(this
.lineWidth),m=0<this.xLabelRotation;b.beginPath();0<d?(b.lineWidth=this
.gridLineWidth,b.strokeStyle=this.gridLineColor):(b.lineWidth=this.lineWidth,b
.strokeStyle=this.lineColor);b.moveTo(n,this.endPoint);b.lineTo(n,this
.startPoint-3);b.stroke();b.closePath();b.lineWidth=this.lineWidth;b
.strokeStyle=this.lineColor;b.beginPath();b.moveTo(n,this.endPoint);
b.lineTo(n,this.endPoint+5);b.stroke();b.closePath();b.save();b.translate(h
,m?this.endPoint+12:this.endPoint+8);b.rotate(-1*F(this.xLabelRotation));b
.font=this.font;b.textAlign=m?"right":"center";b.textBaseline=m?"middle":"top"
;b.fillText(e,0,0);b.restore()},this))}});m.RadialScale=m.Element.extend(
{initialize:function(){this.size=R([this.height,this.width]);this
.drawingArea=this.display?this.size/2-(this.fontSize/2+this.backdropPaddingY
):this.size/2},calculateCenterOffset:function(b){var e=this.drawingArea/
(this.max-this.min);return(b-this.min)*e},update:function(){this.lineArc?this
.drawingArea=this.display?this.size/2-(this.fontSize/2+this.backdropPaddingY
):this.size/2:this.setScaleSize();this.buildYLabels()},buildYLabels:function()
{this.yLabels=[];for(var b=sa(this.stepValue),e=0;e<=this.steps;e++)this
.yLabels.push(S(this.templateString,{value:(this.min+e*this.stepValue).toFixed
(b)}))},getCircumference:function(){return 2*Math.PI/this.valuesCount}
,setScaleSize:function(){var b,e,d,h,n,m,r,p=R([this.height/
2-this.pointLabelFontSize-5,this.width/2]);r=this.width;var v=0;this.ctx
.font=P(this.pointLabelFontSize,this.pointLabelFontStyle,this
.pointLabelFontFamily);for(e=0;e<this.valuesCount;e++)b=this.getPointPosition
(e,p),d=this.ctx.measureText(S(this.templateString,{value:this.labels[e]}))
.width+5,0===e||e===this.valuesCount/2?(h=d/2,b.x+h>r&&(r=b.x+h,n=e),b.x-h<v&&
(v=b.x-h,m=e)):e<this.valuesCount/2?b.x+d>r&&(r=b.x+d,n=e):e>this
.valuesCount/2&&b.x-d<v&&(v=b.x-d,m=e);b=v;r=Math.ceil(r-this.width);n=this
.getIndexAngle(n);
m=this.getIndexAngle(m);n=r/Math.sin(n+Math.PI/2);m=b/Math.sin(m+Math.PI/2)
;n=Q(n)?n:0;m=Q(m)?m:0;this.drawingArea=p-(m+n)/2;this.setCenterPoint(m,n)}
,setCenterPoint:function(b,e){var d=this.width-e-this.drawingArea,h=b+this
.drawingArea;this.xCenter=(h+d)/2;this.yCenter=this.height/2}
,getIndexAngle:function(b){var e=2*Math.PI/this.valuesCount;return b*e-Math
.PI/2},getPointPosition:function(b,e){var d=this.getIndexAngle(b);return
{x:Math.cos(d)*e+this.xCenter,y:Math.sin(d)*e+this.yCenter}},draw:function()
{if(this.display){var b=
this.ctx;if(n(this.yLabels,function(e,d){if(0<d){var h;h=this.drawingArea/this
.steps*d;var n=this.yCenter-h;if(0<this.lineWidth){if(b.strokeStyle=this
.lineColor,b.lineWidth=this.lineWidth,this.lineArc)b.beginPath(),b.arc(this
.xCenter,this.yCenter,h,0,2*Math.PI);else{b.beginPath();for(var m=0;m<this
.valuesCount;m++)h=this.getPointPosition(m,this.calculateCenterOffset(this
.min+d*this.stepValue)),0===m?b.moveTo(h.x,h.y):b.lineTo(h.x,h.y)}b.closePath(
);b.stroke()}if(this.showLabels){if(b.font=P(this.fontSize,
this.fontStyle,this.fontFamily),this.showLabelBackdrop)h=b.measureText(e)
.width,b.fillStyle=this.backdropColor,b.fillRect(this.xCenter-h/2-this
.backdropPaddingX,n-this.fontSize/2-this.backdropPaddingY,h+2*this
.backdropPaddingX,this.fontSize+2*this.backdropPaddingY);b.textAlign="center"
;b.textBaseline="middle";b.fillStyle=this.fontColor;b.fillText(e,this.xCenter
,n)}}},this),!this.lineArc){b.lineWidth=this.angleLineWidth;b.strokeStyle=this
.angleLineColor;for(var e=this.valuesCount-1;0<=e;e--){if(0<
this.angleLineWidth){var d=this.getPointPosition(e,this.calculateCenterOffset
(this.max));b.beginPath();b.moveTo(this.xCenter,this.yCenter);b.lineTo(d.x,d.y
);b.stroke();b.closePath()}d=this.getPointPosition(e,this
.calculateCenterOffset(this.max)+5);b.font=P(this.pointLabelFontSize,this
.pointLabelFontStyle,this.pointLabelFontFamily);b.fillStyle=this
.pointLabelFontColor;var h=this.labels.length,m=this.labels.length/2,r=m/2
,p=r>e||e>h-r,h=e===r||e===h-r;b
.textAlign=0===e?"center":e===m?"center":m>e?"left":
"right";b.textBaseline=h?"middle":p?"bottom":"top";b.fillText(this.labels[e],d
.x,d.y)}}}}});d.addEvent(window,"resize",function(){var b;return function()
{clearTimeout(b);b=setTimeout(function(){n(m.instances,function(b){b.options
.responsive&&b.resize(b.render,!0)})},50)}}());ra?define(function(){return m}
):"object"==typeof module&&module.exports&&(module.exports=m);v.Chart=m;m
.noConflict=function(){return v.Chart=p,m}}).call(this);
(function(){var v=this,p=v.Chart,m=p.helpers,v={scaleBeginAtZero:!0
,scaleShowGridLines:!0,scaleGridLineColor:"rgba(0,0,0,.05)"
,scaleGridLineWidth:1,barShowStroke:!0,barStrokeWidth:2,barValueSpacing:5
,barDatasetSpacing:1,legendTemplate:'<ul class="<%=name.toLowerCase(
)%>-legend"><% for (var i=0; i<datasets.length; i++){%><li><span
 style="background-color:<%=datasets[i].fillColor%>"></span><%if(datasets[i]
.label){%><%=datasets[i].label%><%}%></li><%}%></ul>'};p.Type.extend(
{name:"Bar",defaults:v,initialize:function(d){var n=
this.options;this.ScaleClass=p.Scale.extend({offsetGridLines:!0
,calculateBarX:function(d,m,p){var v=this.calculateBaseWidth();p=this
.calculateX(p)-v/2;d=this.calculateBarWidth(d);return p+d*m+m*n
.barDatasetSpacing+d/2},calculateBaseWidth:function(){return this.calculateX(1
)-this.calculateX(0)-2*n.barValueSpacing},calculateBarWidth:function(d){var
 m=this.calculateBaseWidth()-(d-1)*n.barDatasetSpacing;return m/d}});this
.datasets=[];this.options.showTooltips&&m.bindEvents(this,this.options
.tooltipEvents,
function(d){d="mouseout"!==d.type?this.getBarsAtEvent(d):[];this.eachBars
(function(d){d.restore(["fillColor","strokeColor"])});m.each(d,function(d){d
.fillColor=d.highlightFill;d.strokeColor=d.highlightStroke});this.showTooltip
(d)});this.BarClass=p.Rectangle.extend({strokeWidth:this.options
.barStrokeWidth,showStroke:this.options.barShowStroke,ctx:this.chart.ctx});m
.each(d.datasets,function(h){var n={label:h.label||null,fillColor:h.fillColor
,strokeColor:h.strokeColor,bars:[]};this.datasets.push(n);
m.each(h.data,function(m,p){n.bars.push(new this.BarClass({value:m,label:d
.labels[p],datasetLabel:h.label,strokeColor:h.strokeColor,fillColor:h
.fillColor,highlightFill:h.highlightFill||h.fillColor,highlightStroke:h
.highlightStroke||h.strokeColor}))},this)},this);this.buildScale(d.labels)
;this.BarClass.prototype.base=this.scale.endPoint;this.eachBars(function(d,n,p
){m.extend(d,{width:this.scale.calculateBarWidth(this.datasets.length),x:this
.scale.calculateBarX(this.datasets.length,p,n),y:this.scale.endPoint});
d.save()},this);this.render()},update:function(){this.scale.update();m.each
(this.activeElements,function(d){d.restore(["fillColor","strokeColor"])});this
.eachBars(function(d){d.save()});this.render()},eachBars:function(d){m.each
(this.datasets,function(n,h){m.each(n.bars,d,this,h)},this)}
,getBarsAtEvent:function(d){var n,h=[];d=m.getRelativePosition(d);for(var
 r=function(d){h.push(d.bars[n])},p=0;p<this.datasets.length;p++)for(n=0
;n<this.datasets[p].bars.length;n++)if(this.datasets[p].bars[n].inRange(d.x,
d.y))return m.each(this.datasets,r),h;return h},buildScale:function(d){var
 n=this,h=function(){var d=[];return n.eachBars(function(h){d.push(h.value)})
,d};d={templateString:this.options.scaleLabel,height:this.chart.height
,width:this.chart.width,ctx:this.chart.ctx,textColor:this.options
.scaleFontColor,fontSize:this.options.scaleFontSize,fontStyle:this.options
.scaleFontStyle,fontFamily:this.options.scaleFontFamily,valuesCount:d.length
,beginAtZero:this.options.scaleBeginAtZero,integersOnly:this.options
.scaleIntegersOnly,
calculateYRange:function(d){d=m.calculateScaleRange(h(),d,this.fontSize,this
.beginAtZero,this.integersOnly);m.extend(this,d)},xLabels:d,font:m.fontString
(this.options.scaleFontSize,this.options.scaleFontStyle,this.options
.scaleFontFamily),lineWidth:this.options.scaleLineWidth,lineColor:this.options
.scaleLineColor,gridLineWidth:this.options.scaleShowGridLines?this.options
.scaleGridLineWidth:0,gridLineColor:this.options.scaleShowGridLines?this
.options.scaleGridLineColor:"rgba(0,0,0,0)",padding:this.options.showScale?
0:this.options.barShowStroke?this.options.barStrokeWidth:0,showLabels:this
.options.scaleShowLabels,display:this.options.showScale};this.options
.scaleOverride&&m.extend(d,{calculateYRange:m.noop,steps:this.options
.scaleSteps,stepValue:this.options.scaleStepWidth,min:this.options
.scaleStartValue,max:this.options.scaleStartValue+this.options.scaleSteps*this
.options.scaleStepWidth});this.scale=new this.ScaleClass(d)},addData:function
(d,n){m.each(d,function(d,m){this.datasets[m].bars.push(new this.BarClass(
{value:d,
label:n,x:this.scale.calculateBarX(this.datasets.length,m,this.scale
.valuesCount+1),y:this.scale.endPoint,width:this.scale.calculateBarWidth(this
.datasets.length),base:this.scale.endPoint,strokeColor:this.datasets[m]
.strokeColor,fillColor:this.datasets[m].fillColor}))},this);this.scale
.addXLabel(n);this.update()},removeData:function(){this.scale.removeXLabel();m
.each(this.datasets,function(d){d.bars.shift()},this);this.update()}
,reflow:function(){m.extend(this.BarClass.prototype,{y:this.scale.endPoint,
base:this.scale.endPoint});var d=m.extend({height:this.chart.height,width:this
.chart.width});this.scale.update(d)},draw:function(d){var n=d||1;this.clear()
;this.chart.ctx;this.scale.draw(n);m.each(this.datasets,function(d,p){m.each(d
.bars,function(d,h){d.hasValue()&&(d.base=this.scale.endPoint,d.transition(
{x:this.scale.calculateBarX(this.datasets.length,p,h),y:this.scale.calculateY
(d.value),width:this.scale.calculateBarWidth(this.datasets.length)},n).draw())
},this)},this)}})}).call(this);
(function(){var v=this,p=v.Chart,m=p.helpers,v={segmentShowStroke:!0
,segmentStrokeColor:"#fff",segmentStrokeWidth:2,percentageInnerCutout:50
,animationSteps:100,animationEasing:"easeOutBounce",animateRotate:!0
,animateScale:!1,legendTemplate:'<ul class="<%=name.toLowerCase()%>-legend"><%
 for (var i=0; i<segments.length; i++){%><li><span
 style="background-color:<%=segments[i].fillColor%>"></span><%if(segments[i]
.label){%><%=segments[i].label%><%}%></li><%}%></ul>'};p.Type.extend(
{name:"Doughnut",defaults:v,
initialize:function(d){this.segments=[];this.outerRadius=(m.min([this.chart
.width,this.chart.height])-this.options.segmentStrokeWidth/2)/2;this
.SegmentArc=p.Arc.extend({ctx:this.chart.ctx,x:this.chart.width/2,y:this.chart
.height/2});this.options.showTooltips&&m.bindEvents(this,this.options
.tooltipEvents,function(d){d="mouseout"!==d.type?this.getSegmentsAtEvent(d):[]
;m.each(this.segments,function(d){d.restore(["fillColor"])});m.each(d,function
(d){d.fillColor=d.highlightColor});this.showTooltip(d)});
this.calculateTotal(d);m.each(d,function(d,h){this.addData(d,h,!0)},this);this
.render()},getSegmentsAtEvent:function(d){var n=[],h=m.getRelativePosition(d)
;return m.each(this.segments,function(d){d.inRange(h.x,h.y)&&n.push(d)},this)
,n},addData:function(d,n,h){n=n||this.segments.length;this.segments.splice(n,0
,new this.SegmentArc({value:d.value,outerRadius:this.options
.animateScale?0:this.outerRadius,innerRadius:this.options.animateScale?0:this
.outerRadius/100*this.options.percentageInnerCutout,fillColor:d.color,
highlightColor:d.highlight||d.color,showStroke:this.options.segmentShowStroke
,strokeWidth:this.options.segmentStrokeWidth,strokeColor:this.options
.segmentStrokeColor,startAngle:1.5*Math.PI,circumference:this.options
.animateRotate?0:this.calculateCircumference(d.value),label:d.label}));h||
(this.reflow(),this.update())},calculateCircumference:function(d){return
 d/this.total*Math.PI*2},calculateTotal:function(d){this.total=0;m.each(d
,function(d){this.total+=d.value},this)},update:function(){this.calculateTotal
(this.segments);
m.each(this.activeElements,function(d){d.restore(["fillColor"])});m.each(this
.segments,function(d){d.save()});this.render()},removeData:function(d){d=m
.isNumber(d)?d:this.segments.length-1;this.segments.splice(d,1);this.reflow()
;this.update()},reflow:function(){m.extend(this.SegmentArc.prototype,{x:this
.chart.width/2,y:this.chart.height/2});this.outerRadius=(m.min([this.chart
.width,this.chart.height])-this.options.segmentStrokeWidth/2)/2;m.each(this
.segments,function(d){d.update({outerRadius:this.outerRadius,
innerRadius:this.outerRadius/100*this.options.percentageInnerCutout})},this)}
,draw:function(d){var n=d?d:1;this.clear();m.each(this.segments,function(d,m)
{d.transition({circumference:this.calculateCircumference(d.value)
,outerRadius:this.outerRadius,innerRadius:this.outerRadius/100*this.options
.percentageInnerCutout},n);d.endAngle=d.startAngle+d.circumference;d.draw()
;0===m&&(d.startAngle=1.5*Math.PI);m<this.segments.length-1&&(this.segments
[m+1].startAngle=d.endAngle)},this)}});p.types.Doughnut.extend({name:"Pie",
defaults:m.merge(v,{percentageInnerCutout:0})})}).call(this);
(function(){var v=this,p=v.Chart,m=p.helpers,v={scaleShowGridLines:!0
,scaleGridLineColor:"rgba(0,0,0,.05)",scaleGridLineWidth:1,bezierCurve:!0
,bezierCurveTension:.4,pointDot:!0,pointDotRadius:4,pointDotStrokeWidth:1
,pointHitDetectionRadius:20,datasetStroke:!0,datasetStrokeWidth:2
,datasetFill:!0,legendTemplate:'<ul class="<%=name.toLowerCase()%>-legend"><%
 for (var i=0; i<datasets.length; i++){%><li><span
 style="background-color:<%=datasets[i].strokeColor%>"></span><%if(datasets[i]
.label){%><%=datasets[i].label%><%}%></li><%}%></ul>'};p.Type.extend(
{name:"Line",
defaults:v,initialize:function(d){this.PointClass=p.Point.extend(
{strokeWidth:this.options.pointDotStrokeWidth,radius:this.options
.pointDotRadius,display:this.options.pointDot,hitDetectionRadius:this.options
.pointHitDetectionRadius,ctx:this.chart.ctx,inRange:function(d){return Math
.pow(d-this.x,2)<Math.pow(this.radius+this.hitDetectionRadius,2)}});this
.datasets=[];this.options.showTooltips&&m.bindEvents(this,this.options
.tooltipEvents,function(d){d="mouseout"!==d.type?this.getPointsAtEvent(d):[];
this.eachPoints(function(d){d.restore(["fillColor","strokeColor"])});m.each(d
,function(d){d.fillColor=d.highlightFill;d.strokeColor=d.highlightStroke})
;this.showTooltip(d)});m.each(d.datasets,function(n){var h={label:n.label|
|null,fillColor:n.fillColor,strokeColor:n.strokeColor,pointColor:n.pointColor
,pointStrokeColor:n.pointStrokeColor,points:[]};this.datasets.push(h);m.each(n
.data,function(m,p){h.points.push(new this.PointClass({value:m,label:d.labels
[p],datasetLabel:n.label,strokeColor:n.pointStrokeColor,
fillColor:n.pointColor,highlightFill:n.pointHighlightFill||n.pointColor
,highlightStroke:n.pointHighlightStroke||n.pointStrokeColor}))},this);this
.buildScale(d.labels);this.eachPoints(function(d,h){m.extend(d,{x:this.scale
.calculateX(h),y:this.scale.endPoint});d.save()},this)},this);this.render()}
,update:function(){this.scale.update();m.each(this.activeElements,function(d)
{d.restore(["fillColor","strokeColor"])});this.eachPoints(function(d){d.save()
});this.render()},eachPoints:function(d){m.each(this.datasets,
function(n){m.each(n.points,d,this)},this)},getPointsAtEvent:function(d){var
 n=[],h=m.getRelativePosition(d);return m.each(this.datasets,function(d){m
.each(d.points,function(d){d.inRange(h.x,h.y)&&n.push(d)})},this),n}
,buildScale:function(d){var n=this,h=function(){var d=[];return n.eachPoints
(function(h){d.push(h.value)}),d};d={templateString:this.options.scaleLabel
,height:this.chart.height,width:this.chart.width,ctx:this.chart.ctx
,textColor:this.options.scaleFontColor,fontSize:this.options.scaleFontSize,
fontStyle:this.options.scaleFontStyle,fontFamily:this.options.scaleFontFamily
,valuesCount:d.length,beginAtZero:this.options.scaleBeginAtZero
,integersOnly:this.options.scaleIntegersOnly,calculateYRange:function(d){d=m
.calculateScaleRange(h(),d,this.fontSize,this.beginAtZero,this.integersOnly);m
.extend(this,d)},xLabels:d,font:m.fontString(this.options.scaleFontSize,this
.options.scaleFontStyle,this.options.scaleFontFamily),lineWidth:this.options
.scaleLineWidth,lineColor:this.options.scaleLineColor,gridLineWidth:this
.options.scaleShowGridLines?
this.options.scaleGridLineWidth:0,gridLineColor:this.options
.scaleShowGridLines?this.options.scaleGridLineColor:"rgba(0,0,0,0)"
,padding:this.options.showScale?0:this.options.pointDotRadius+this.options
.pointDotStrokeWidth,showLabels:this.options.scaleShowLabels,display:this
.options.showScale};this.options.scaleOverride&&m.extend(d,{calculateYRange:m
.noop,steps:this.options.scaleSteps,stepValue:this.options.scaleStepWidth
,min:this.options.scaleStartValue,max:this.options.scaleStartValue+this
.options.scaleSteps*
this.options.scaleStepWidth});this.scale=new p.Scale(d)},addData:function(d,n)
{m.each(d,function(d,m){this.datasets[m].points.push(new this.PointClass(
{value:d,label:n,x:this.scale.calculateX(this.scale.valuesCount+1),y:this
.scale.endPoint,strokeColor:this.datasets[m].pointStrokeColor,fillColor:this
.datasets[m].pointColor}))},this);this.scale.addXLabel(n);this.update()}
,removeData:function(){this.scale.removeXLabel();m.each(this.datasets,function
(d){d.points.shift()},this);this.update()},reflow:function(){var d=
m.extend({height:this.chart.height,width:this.chart.width});this.scale.update
(d)},draw:function(d){var n=d||1;this.clear();var h=this.chart.ctx,p=function
(d){return null!==d.value},v=function(d,h,n){return m.findNextWhere(h,p,n)||d}
,D=function(d,h,n){return m.findPreviousWhere(h,p,n)||d};this.scale.draw(n);m
.each(this.datasets,function(d){var E=m.where(d.points,p);m.each(d.points
,function(d,h){d.hasValue()&&d.transition({y:this.scale.calculateY(d.value)
,x:this.scale.calculateX(h)},n)},this);this.options.bezierCurve&&
m.each(E,function(d,h){var n=0<h&&h<E.length-1?this.options
.bezierCurveTension:0;d.controlPoints=m.splineCurve(D(d,E,h),d,v(d,E,h),n);d
.controlPoints.outer.y>this.scale.endPoint?d.controlPoints.outer.y=this.scale
.endPoint:d.controlPoints.outer.y<this.scale.startPoint&&(d.controlPoints
.outer.y=this.scale.startPoint);d.controlPoints.inner.y>this.scale.endPoint?d
.controlPoints.inner.y=this.scale.endPoint:d.controlPoints.inner.y<this.scale
.startPoint&&(d.controlPoints.inner.y=this.scale.startPoint)},
this);h.lineWidth=this.options.datasetStrokeWidth;h.strokeStyle=d.strokeColor
;h.beginPath();m.each(E,function(d,n){if(0===n)h.moveTo(d.x,d.y);else if(this
.options.bezierCurve){var m=D(d,E,n);h.bezierCurveTo(m.controlPoints.outer.x,m
.controlPoints.outer.y,d.controlPoints.inner.x,d.controlPoints.inner.y,d.x,d.y
)}else h.lineTo(d.x,d.y)},this);h.stroke();this.options.datasetFill&&0<E
.length&&(h.lineTo(E[E.length-1].x,this.scale.endPoint),h.lineTo(E[0].x,this
.scale.endPoint),h.fillStyle=d.fillColor,h.closePath(),
h.fill());m.each(E,function(d){d.draw()})},this)}})}).call(this);
(function(){var v=this,p=v.Chart,m=p.helpers,v={scaleShowLabelBackdrop:!0
,scaleBackdropColor:"rgba(255,255,255,0.75)",scaleBeginAtZero:!0
,scaleBackdropPaddingY:2,scaleBackdropPaddingX:2,scaleShowLine:!0
,segmentShowStroke:!0,segmentStrokeColor:"#fff",segmentStrokeWidth:2
,animationSteps:100,animationEasing:"easeOutBounce",animateRotate:!0
,animateScale:!1,legendTemplate:'<ul class="<%=name.toLowerCase()%>-legend"><%
 for (var i=0; i<segments.length; i++){%><li><span
 style="background-color:<%=segments[i].fillColor%>"></span><%if(segments[i]
.label){%><%=segments[i].label%><%}%></li><%}%></ul>'};p.Type.extend(
{name:"PolarArea",
defaults:v,initialize:function(d){this.segments=[];this.SegmentArc=p.Arc
.extend({showStroke:this.options.segmentShowStroke,strokeWidth:this.options
.segmentStrokeWidth,strokeColor:this.options.segmentStrokeColor,ctx:this.chart
.ctx,innerRadius:0,x:this.chart.width/2,y:this.chart.height/2});this.scale=new
 p.RadialScale({display:this.options.showScale,fontStyle:this.options
.scaleFontStyle,fontSize:this.options.scaleFontSize,fontFamily:this.options
.scaleFontFamily,fontColor:this.options.scaleFontColor,
showLabels:this.options.scaleShowLabels,showLabelBackdrop:this.options
.scaleShowLabelBackdrop,backdropColor:this.options.scaleBackdropColor
,backdropPaddingY:this.options.scaleBackdropPaddingY,backdropPaddingX:this
.options.scaleBackdropPaddingX,lineWidth:this.options.scaleShowLine?this
.options.scaleLineWidth:0,lineColor:this.options.scaleLineColor,lineArc:!0
,width:this.chart.width,height:this.chart.height,xCenter:this.chart.width/2
,yCenter:this.chart.height/2,ctx:this.chart.ctx,templateString:this.options
.scaleLabel,
valuesCount:d.length});this.updateScaleRange(d);this.scale.update();m.each(d
,function(d,h){this.addData(d,h,!0)},this);this.options.showTooltips&&m
.bindEvents(this,this.options.tooltipEvents,function(d){d="mouseout"!==d
.type?this.getSegmentsAtEvent(d):[];m.each(this.segments,function(d){d.restore
(["fillColor"])});m.each(d,function(d){d.fillColor=d.highlightColor});this
.showTooltip(d)});this.render()},getSegmentsAtEvent:function(d){var n=[],h=m
.getRelativePosition(d);return m.each(this.segments,function(d){d.inRange(h.x,
h.y)&&n.push(d)},this),n},addData:function(d,n,h){n=n||this.segments.length
;this.segments.splice(n,0,new this.SegmentArc({fillColor:d.color
,highlightColor:d.highlight||d.color,label:d.label,value:d.value
,outerRadius:this.options.animateScale?0:this.scale.calculateCenterOffset(d
.value),circumference:this.options.animateRotate?0:this.scale.getCircumference
(),startAngle:1.5*Math.PI}));h||(this.reflow(),this.update())}
,removeData:function(d){d=m.isNumber(d)?d:this.segments.length-1;this.segments
.splice(d,
1);this.reflow();this.update()},calculateTotal:function(d){this.total=0;m.each
(d,function(d){this.total+=d.value},this);this.scale.valuesCount=this.segments
.length},updateScaleRange:function(d){var n=[];m.each(d,function(d){n.push(d
.value)});d=this.options.scaleOverride?{steps:this.options.scaleSteps
,stepValue:this.options.scaleStepWidth,min:this.options.scaleStartValue
,max:this.options.scaleStartValue+this.options.scaleSteps*this.options
.scaleStepWidth}:m.calculateScaleRange(n,m.min([this.chart.width,
this.chart.height])/2,this.options.scaleFontSize,this.options.scaleBeginAtZero
,this.options.scaleIntegersOnly);m.extend(this.scale,d,{size:m.min([this.chart
.width,this.chart.height]),xCenter:this.chart.width/2,yCenter:this.chart
.height/2})},update:function(){this.calculateTotal(this.segments);m.each(this
.segments,function(d){d.save()});this.render()},reflow:function(){m.extend
(this.SegmentArc.prototype,{x:this.chart.width/2,y:this.chart.height/2});this
.updateScaleRange(this.segments);this.scale.update();
m.extend(this.scale,{xCenter:this.chart.width/2,yCenter:this.chart.height/2})
;m.each(this.segments,function(d){d.update({outerRadius:this.scale
.calculateCenterOffset(d.value)})},this)},draw:function(d){var n=d||1;this
.clear();m.each(this.segments,function(d,m){d.transition({circumference:this
.scale.getCircumference(),outerRadius:this.scale.calculateCenterOffset(d.value
)},n);d.endAngle=d.startAngle+d.circumference;0===m&&(d.startAngle=1.5*Math.PI
);m<this.segments.length-1&&(this.segments[m+1].startAngle=
d.endAngle);d.draw()},this);this.scale.draw()}})}).call(this);
(function(){var v=this,p=v.Chart,m=p.helpers;p.Type.extend({name:"Radar"
,defaults:{scaleShowLine:!0,angleShowLineOut:!0,scaleShowLabels:!1
,scaleBeginAtZero:!0,angleLineColor:"rgba(0,0,0,.1)",angleLineWidth:1
,pointLabelFontFamily:"'Arial'",pointLabelFontStyle:"normal"
,pointLabelFontSize:10,pointLabelFontColor:"#666",pointDot:!0,pointDotRadius:3
,pointDotStrokeWidth:1,pointHitDetectionRadius:20,datasetStroke:!0
,datasetStrokeWidth:2,datasetFill:!0,legendTemplate:'<ul class="<%=name
.toLowerCase()%>-legend"><% for (var i=0; i<datasets.length; i++){%><li><span
 style="background-color:<%=datasets[i].strokeColor%>"></span><%if(datasets[i]
.label){%><%=datasets[i].label%><%}%></li><%}%></ul>'},
initialize:function(d){this.PointClass=p.Point.extend({strokeWidth:this
.options.pointDotStrokeWidth,radius:this.options.pointDotRadius,display:this
.options.pointDot,hitDetectionRadius:this.options.pointHitDetectionRadius
,ctx:this.chart.ctx});this.datasets=[];this.buildScale(d);this.options
.showTooltips&&m.bindEvents(this,this.options.tooltipEvents,function(d)
{d="mouseout"!==d.type?this.getPointsAtEvent(d):[];this.eachPoints(function(d)
{d.restore(["fillColor","strokeColor"])});m.each(d,function(d){d.fillColor=
d.highlightFill;d.strokeColor=d.highlightStroke});this.showTooltip(d)});m.each
(d.datasets,function(n){var h={label:n.label||null,fillColor:n.fillColor
,strokeColor:n.strokeColor,pointColor:n.pointColor,pointStrokeColor:n
.pointStrokeColor,points:[]};this.datasets.push(h);m.each(n.data,function(m,p)
{var v;this.scale.animation||(v=this.scale.getPointPosition(p,this.scale
.calculateCenterOffset(m)));h.points.push(new this.PointClass({value:m,label:d
.labels[p],datasetLabel:n.label,x:this.options.animation?
this.scale.xCenter:v.x,y:this.options.animation?this.scale.yCenter:v.y
,strokeColor:n.pointStrokeColor,fillColor:n.pointColor,highlightFill:n
.pointHighlightFill||n.pointColor,highlightStroke:n.pointHighlightStroke||n
.pointStrokeColor}))},this)},this);this.render()},eachPoints:function(d){m
.each(this.datasets,function(n){m.each(n.points,d,this)},this)}
,getPointsAtEvent:function(d){d=m.getRelativePosition(d);d=m.getAngleFromPoint
({x:this.scale.xCenter,y:this.scale.yCenter},d);var n=2*Math.PI/this.scale
.valuesCount,
h=Math.round((d.angle-1.5*Math.PI)/n),p=[];return(h>=this.scale.valuesCount|
|0>h)&&(h=0),d.distance<=this.scale.drawingArea&&m.each(this.datasets,function
(d){p.push(d.points[h])}),p},buildScale:function(d){this.scale=new p
.RadialScale({display:this.options.showScale,fontStyle:this.options
.scaleFontStyle,fontSize:this.options.scaleFontSize,fontFamily:this.options
.scaleFontFamily,fontColor:this.options.scaleFontColor,showLabels:this.options
.scaleShowLabels,showLabelBackdrop:this.options.scaleShowLabelBackdrop,
backdropColor:this.options.scaleBackdropColor,backdropPaddingY:this.options
.scaleBackdropPaddingY,backdropPaddingX:this.options.scaleBackdropPaddingX
,lineWidth:this.options.scaleShowLine?this.options.scaleLineWidth:0
,lineColor:this.options.scaleLineColor,angleLineColor:this.options
.angleLineColor,angleLineWidth:this.options.angleShowLineOut?this.options
.angleLineWidth:0,pointLabelFontColor:this.options.pointLabelFontColor
,pointLabelFontSize:this.options.pointLabelFontSize,pointLabelFontFamily:this
.options.pointLabelFontFamily,
pointLabelFontStyle:this.options.pointLabelFontStyle,height:this.chart.height
,width:this.chart.width,xCenter:this.chart.width/2,yCenter:this.chart.height/2
,ctx:this.chart.ctx,templateString:this.options.scaleLabel,labels:d.labels
,valuesCount:d.datasets[0].data.length});this.scale.setScaleSize();this
.updateScaleRange(d.datasets);this.scale.buildYLabels()}
,updateScaleRange:function(d){var n=function(){var h=[];return m.each(d
,function(d){d.data?h=h.concat(d.data):m.each(d.points,function(d){h.push(d
.value)})}),
h}(),n=this.options.scaleOverride?{steps:this.options.scaleSteps
,stepValue:this.options.scaleStepWidth,min:this.options.scaleStartValue
,max:this.options.scaleStartValue+this.options.scaleSteps*this.options
.scaleStepWidth}:m.calculateScaleRange(n,m.min([this.chart.width,this.chart
.height])/2,this.options.scaleFontSize,this.options.scaleBeginAtZero,this
.options.scaleIntegersOnly);m.extend(this.scale,n)},addData:function(d,n){this
.scale.valuesCount++;m.each(d,function(d,m){var p=this.scale.getPointPosition
(this.scale.valuesCount,
this.scale.calculateCenterOffset(d));this.datasets[m].points.push(new this
.PointClass({value:d,label:n,x:p.x,y:p.y,strokeColor:this.datasets[m]
.pointStrokeColor,fillColor:this.datasets[m].pointColor}))},this);this.scale
.labels.push(n);this.reflow();this.update()},removeData:function(){this.scale
.valuesCount--;this.scale.labels.shift();m.each(this.datasets,function(d){d
.points.shift()},this);this.reflow();this.update()},update:function(){this
.eachPoints(function(d){d.save()});this.reflow();this.render()},
reflow:function(){m.extend(this.scale,{width:this.chart.width,height:this
.chart.height,size:m.min([this.chart.width,this.chart.height]),xCenter:this
.chart.width/2,yCenter:this.chart.height/2});this.updateScaleRange(this
.datasets);this.scale.setScaleSize();this.scale.buildYLabels()},draw:function
(d){var n=d||1,h=this.chart.ctx;this.clear();this.scale.draw();m.each(this
.datasets,function(d){m.each(d.points,function(d,h){d.hasValue()&&d.transition
(this.scale.getPointPosition(h,this.scale.calculateCenterOffset(d.value)),
n)},this);h.lineWidth=this.options.datasetStrokeWidth;h.strokeStyle=d
.strokeColor;h.beginPath();m.each(d.points,function(d,m){0===m?h.moveTo(d.x,d
.y):h.lineTo(d.x,d.y)},this);h.closePath();h.stroke();h.fillStyle=d.fillColor
;h.fill();m.each(d.points,function(d){d.hasValue()&&d.draw()})},this)}})})
.call(this);
