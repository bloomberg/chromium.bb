// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var naclModule = null;
var presets = [
  [[15.2,32.1,7.6],[3,0.329,0.15,0.321,0.145,0.709,3,2,4,0.269,0.662],[0,"#000000",3,"#f5f5c1",12,"#158a34",68,"#89e681",100]],
  [[15.2,32.1,5],[3,0.273,0.117,0.288,0.243,0.348,3,2,4,0.269,0.662],[1,"#000000",3,"#f5f5c1",8,"#158a34",17,"#89e681",20]],
  [[4,12,1],[2,0.115,0.269,0.523,0.34,0.746,3,4,4,0.028,0.147],[0,"#36065e",0,"#c24242",77,"#8a19b0",91,"#ff9900",99,"#f5c816",99]],
  [[4,12,1],[3,0.12,0.218,0.267,0.365,0.445,3,4,4,0.028,0.147],[0,"#000000",0,"#0f8a84",38,"#f5f5c1",43,"#158a34",70,"#89e681",100]],
  [[4,12,1],[0,0.09,0.276,0.27,0.365,0.445,1,4,4,0.028,0.147],[0,"#93afd9",11,"#9cf0ff",92,"#edfdff",100]],
  [[10.4,12,1],[2,0.082,0.302,0.481,0.35,0.749,2,3,4,0.028,0.147],[0,"#000000",11,"#ffffff",22,"#19a68a",85,"#6b0808",98]],
  [[7.8,27.2,2.6],[3,0.21,0.714,0.056,0.175,0.838,2,0,2,0.132,0.311],[0,"#0a1340",0,"#ffffff",55,"#4da8a3",83,"#2652ab",99,"#2f1e75",46]],
  [[4,12,1],[2,0.115,0.269,0.496,0.34,0.767,3,4,4,0.028,0.147],[0,"#b8cfcf",0,"#3f5a5c",77,"#1a330a",91,"#c0e0dc",99]],
  [[10.6,31.8,1],[1,0.157,0.092,0.256,0.098,0.607,3,4,4,0.015,0.34],[0,"#4d3e3e",0,"#9a1ac9",77,"#aaf09e",100]],
];

/**
 * A helper function to abbreviate getElementById.
 *
 * @param {string} elementId The id to get.
 * @return {Element}
 */
function $(elementId) {
  return document.getElementById(elementId);
}

/**
 * MIME type for PNaCl
 *
 * @return {string} MIME type
 */
function PNaClmimeType() {
  return 'application/x-pnacl';
}

/**
 * Check if the browser supports PNaCl.
 *
 * @return {bool}
 */
function browserSupportsPNaCl() {
  var mimetype = PNaClmimeType();
  return navigator.mimeTypes[mimetype] !== undefined;
}

/**
 * Get the URL for Google Cloud Storage.
 *
 * @param {string} name The relative path to the file.
 * @return {string}
 */
function getDataURL(name) {
  var revision = 233080;
  var baseUrl = 'http://commondatastorage.googleapis.com/gonacl/demos/publish/';
  return baseUrl + revision + '/smoothlife/' + name;
}

/**
 * Create the Native Client <embed> element as a child of the DOM element
 * named "listener".
 *
 * @param {string} name The name of the example.
 * @param {number} width The width to create the plugin.
 * @param {number} height The height to create the plugin.
 * @param {Object} attrs Dictionary of attributes to set on the module.
 */
function createNaClModule(name, width, height, attrs) {
  var moduleEl = document.createElement('embed');
  moduleEl.setAttribute('name', 'nacl_module');
  moduleEl.setAttribute('id', 'nacl_module');
  moduleEl.setAttribute('width', width);
  moduleEl.setAttribute('height', height);
  moduleEl.setAttribute('path', '');
  moduleEl.setAttribute('src', getDataURL(name + '.nmf'));
  moduleEl.setAttribute('type', PNaClmimeType());

  // Add any optional arguments
  if (attrs) {
    for (var key in attrs) {
      moduleEl.setAttribute(key, attrs[key]);
    }
  }

  // The <EMBED> element is wrapped inside a <DIV>, which has both a 'load'
  // and a 'message' event listener attached.  This wrapping method is used
  // instead of attaching the event listeners directly to the <EMBED> element
  // to ensure that the listeners are active before the NaCl module 'load'
  // event fires.
  var listenerDiv = $('listener');
  listenerDiv.appendChild(moduleEl);
}

/**
 * Add the default event listeners to the element with id "listener".
 */
function attachDefaultListeners() {
  var listenerDiv = $('listener');
  listenerDiv.addEventListener('load', moduleDidLoad, true);
  listenerDiv.addEventListener('error', moduleLoadError, true);
  listenerDiv.addEventListener('progress', moduleLoadProgress, true);
  listenerDiv.addEventListener('message', handleMessage, true);
  listenerDiv.addEventListener('crash', handleCrash, true);
  attachListeners();
}

/**
 * Called when the Browser can not communicate with the Module
 *
 * This event listener is registered in attachDefaultListeners above.
 *
 * @param {Object} event
 */
function handleCrash(event) {
  if (naclModule.exitStatus == -1) {
    updateStatus('CRASHED');
  } else {
    updateStatus('EXITED [' + naclModule.exitStatus + ']');
  }
}

/**
 * Called when the NaCl module is loaded.
 *
 * This event listener is registered in attachDefaultListeners above.
 */
function moduleDidLoad() {
  var bar = $('progress-bar');
  bar.style.width = 100;
  naclModule = $('nacl_module');
  hideStatus();
  setSize(256);
  setThreadCount(4);
  setMaxScale(1);
  loadPreset(0);
}

/**
 * Hide the status field and progress bar.
 */
function hideStatus() {
  $('loading-cover').style.display = 'none';
}

/**
 * Called when the plugin fails to load.
 *
 * @param {Object} event
 */
function moduleLoadError(event) {
  updateStatus('Load failed.');
}

/**
 * Called when the plugin reports progress events.
 *
 * @param {Object} event
 */
function moduleLoadProgress(event) {
  $('progress').style.display = 'block';

  var loadPercent = 0.0;
  var bar = $('progress-bar');

  if (event.lengthComputable && event.total > 0) {
    loadPercent = event.loaded / event.total * 100.0;
  } else {
    // The total length is not yet known.
    loadPercent = 10;
  }
  bar.style.width = loadPercent + "%";
}

/**
 * If the element with id 'statusField' exists, then set its HTML to the status
 * message as well.
 *
 * @param {string} opt_message The message to set.
 */
function updateStatus(opt_message) {
  var statusField = $('statusField');
  if (statusField) {
    statusField.style.display = 'block';
    statusField.textContent = opt_message;
  }
}

/**
 * Add event listeners after the NaCl module has loaded.  These listeners will
 * forward messages to the NaCl module via postMessage()
 */
function attachListeners() {
  $('preset').addEventListener('change', loadSelectedPreset);
  $('reset').addEventListener('click', loadSelectedPreset);
  $('clear').addEventListener('click', function() { clear(0); });
  $('splat').addEventListener('click', function() { splat(); });
  $('brushSizeRange').addEventListener('change', function() {
    var radius = parseFloat(this.value);
    setBrushSize(radius, 1.0);
    $('brushSize').textContent = radius.toFixed(1);
  });
}

function loadSelectedPreset() {
  loadPreset($('preset').value);
}

function loadPreset(index) {
  var preset = presets[index];

  clear(0);
  setKernel.apply(null, preset[0]);
  setSmoother.apply(null, preset[1]);
  setPalette.apply(null, preset[2]);
  splat();
}

function clear(color) {
  naclModule.postMessage({cmd: 'clear', color: color});
}

function setSize(size) {
  naclModule.postMessage({cmd: 'setSize', size: size});
}

function setMaxScale(scale) {
  naclModule.postMessage({cmd: 'setMaxScale', scale: scale});
}

function setThreadCount(threadCount) {
  naclModule.postMessage({cmd: 'setThreadCount', threadCount: threadCount});
}


function setKernel(discRadius, ringRadius, blendRadius) {
  naclModule.postMessage({
    cmd: 'setKernel',
    discRadius: discRadius,
    ringRadius: ringRadius,
    blendRadius: blendRadius});
}

function setSmoother(type, dt, b1, d1, b2, d2, mode, sigmoid, mix, sn, sm) {
  naclModule.postMessage({
    cmd: 'setSmoother',
    type: type, dt: dt,
    b1: b1, d1: d1, b2: b2, d2: d2,
    mode: mode, sigmoid: sigmoid, mix: mix,
    sn: sn, sm: sm});
}

function setPalette() {
  var repeating = arguments[0] !== 0;
  var colors = []
  var stops = []
  for (var i = 1; i < arguments.length; i += 2) {
    colors.push(arguments[i]);
    stops.push(arguments[i + 1]);
  }
  naclModule.postMessage({
    cmd: 'setPalette',
    repeating: repeating,
    colors: colors,
    stops: stops});
}

function splat() {
  naclModule.postMessage({cmd: 'splat'});
}

function setBrushSize(radius, color) {
  naclModule.postMessage({cmd: 'setBrush', radius: radius, color: color});
}


/**
 * Handle a message coming from the NaCl module.
 * @param {Object} message_event
 */
function handleMessage(message_event) {
  // Update FPS
  $('fps').textContent = message_event.data.toFixed(1);
}

/**
 * Listen for the DOM content to be loaded. This event is fired when parsing of
 * the page's document has finished.
 */
document.addEventListener('DOMContentLoaded', function() {
  updateStatus('Loading...');
  if (!browserSupportsPNaCl()) {
    updateStatus('Browser does not support PNaCl or PNaCl is disabled');
  } else if (naclModule == null) {
    createNaClModule('smoothnacl', '100%', '100%');
    attachDefaultListeners();
  } else {
    // It's possible that the Native Client module onload event fired
    // before the page's onload event.  In this case, the status message
    // will reflect 'SUCCESS', but won't be displayed.  This call will
    // display the current message.
    updateStatus('Waiting.');
  }
});
