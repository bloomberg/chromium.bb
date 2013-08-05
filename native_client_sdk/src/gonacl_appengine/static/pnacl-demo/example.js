// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var naclModule = null;

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
  moduleEl.setAttribute('src', name + '.nmf');
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
  var listenerDiv = document.getElementById('listener');
  listenerDiv.appendChild(moduleEl);
}

/**
 * Add the default event listeners to the element with id "listener".
 */
function attachDefaultListeners() {
  var listenerDiv = document.getElementById('listener');
  listenerDiv.addEventListener('load', moduleDidLoad, true);
  listenerDiv.addEventListener('progress', moduleLoadProgress, true);
  listenerDiv.addEventListener('message', handleMessage, true);
  listenerDiv.addEventListener('crash', handleCrash, true);
  if (typeof window.attachListeners !== 'undefined') {
    window.attachListeners();
  }
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
  if (typeof window.handleCrash !== 'undefined') {
    window.handleCrash(naclModule.lastError);
  }
}

/**
 * Called when the NaCl module is loaded.
 *
 * This event listener is registered in attachDefaultListeners above.
 */
function moduleDidLoad() {
  var bar = document.getElementById('progress');
  bar.value = 100;
  bar.max = 100;
  naclModule = document.getElementById('nacl_module');
  updateStatus('RUNNING');

  setAuxElementsVisibility(true);
}

/**
 * Called when the plugin reports progress events.
 *
 * @param {Object} event
 */
function moduleLoadProgress(event) {
  var loadPercent = 0.0;
  var bar = document.getElementById('progress');
  bar.max = 100;
  if (event.lengthComputable && event.total > 0) {
    loadPercent = event.loaded / event.total * 100.0;
  } else {
    // The total length is not yet known.
    loadPercent = -1.0;
  }
  bar.value = loadPercent;
}

/**
 * Return true when |s| starts with the string |prefix|.
 *
 * @param {string} s The string to search.
 * @param {string} prefix The prefix to search for in |s|.
 * @return {bool}
 */
function startsWith(s, prefix) {
  // indexOf would search the entire string, lastIndexOf(p, 0) only checks at
  // the first index. See: http://stackoverflow.com/a/4579228
  return s.lastIndexOf(prefix, 0) === 0;
}

/**
 * Add a message to an element with id "log".
 *
 * This function is used by the default "log:" message handler.
 *
 * @param {string} message The message to log.
 */
function logMessage(message) {
  document.getElementById('log').textContent = logMessageArray.join('');
  console.log(message);
}

var defaultMessageTypes = {
  'alert': alert,
  'log': logMessage
};

/**
 * Called when the NaCl module sends a message to JavaScript (via
 * PPB_Messaging.PostMessage())
 *
 * This event listener is registered in createNaClModule above.
 *
 * @param {Event} message_event A message event. message_event.data contains
 *     the data sent from the NaCl module.
 */
function handleMessage(message_event) {
  if (typeof message_event.data === 'string') {
    for (var type in defaultMessageTypes) {
      if (defaultMessageTypes.hasOwnProperty(type)) {
        if (startsWith(message_event.data, type + ':')) {
          func = defaultMessageTypes[type];
          func(message_event.data.slice(type.length + 1));
          return;
        }
      }
    }
  }

  if (typeof window.handleMessage !== 'undefined') {
    window.handleMessage(message_event);
    return;
  }

  logMessage('Unhandled message: ' + message_event.data + '\n');
}

/**
 * Called when the DOM content has loaded; i.e. the page's document is fully
 * parsed. At this point, we can safely query any elements in the document via
 * document.querySelector, document.getElementById, etc.
 *
 * @param {string} name The name of the example.
 * @param {number} width The width to create the plugin.
 * @param {number} height The height to create the plugin.
 * @param {Object} attrs Optional dictionary of additional attributes.
 */
function domContentLoaded(name, width, height, attrs) {
  // If the page loads before the Native Client module loads, then set the
  // status message indicating that the module is still loading.  Otherwise,
  // do not change the status message.
  setAuxElementsVisibility(false);
  updateStatus('Page loaded.');
  if (!browserSupportsPNaCl()) {
    updateStatus('Browser does not support PNaCl or PNaCl is disabled');
  } else if (naclModule == null) {
    // We use a non-zero sized embed to give Chrome space to place the bad
    // plug-in graphic, if there is a problem.
    width = typeof width !== 'undefined' ? width : 200;
    height = typeof height !== 'undefined' ? height : 200;
    createNaClModule(name, width, height, attrs);
    attachDefaultListeners();
  } else {
    // It's possible that the Native Client module onload event fired
    // before the page's onload event.  In this case, the status message
    // will reflect 'SUCCESS', but won't be displayed.  This call will
    // display the current message.
    updateStatus('Waiting.');
  }
}

/**
 * If the element with id 'statusField' exists, then set its HTML to the status
 * message as well.
 *
 * @param {string} opt_message The message to set.
 */
function updateStatus(opt_message) {
  var statusField = document.getElementById('statusField');
  if (statusField) {
    statusField.innerHTML = opt_message;
  }
}

function postThreadFunc(numThreads) {
  return function() {
    naclModule.postMessage({'message' : 'set_threads',
                            'value' : numThreads});
  }
}

// Add event listeners after the NaCl module has loaded.  These listeners will
// forward messages to the NaCl module via postMessage()
function attachListeners() {
  var threads = [0, 1, 2, 4, 6, 8, 12, 16, 24, 32];
  for (var i = 0; i < threads.length; i++) {
    document.getElementById('radio' + i).addEventListener('click',
        postThreadFunc(threads[i]));
  }
  document.getElementById('zoomRange').addEventListener('change',
    function() {
      var value = parseFloat(document.getElementById('zoomRange').value);
      naclModule.postMessage({'message' : 'set_zoom',
                              'value' : value});
    });
  document.getElementById('lightRange').addEventListener('change',
    function() {
      var value = parseFloat(document.getElementById('lightRange').value);
      naclModule.postMessage({'message' : 'set_light',
                              'value' : value});
    });
}

// Load a texture and send pixel data down to NaCl module.
function loadTexture(name) {
  // Load image from jpg, decompress into canvas.
  var img = new Image();
  img.onload = function() {
    var graph = document.createElement('canvas');
    graph.width = img.width;
    graph.height = img.height;
    var context = graph.getContext('2d');
    context.drawImage(img, 0, 0);
    var imageData = context.getImageData(0, 0, img.width, img.height);
    // Send NaCl module the raw image data obtained from canvas.
    naclModule.postMessage({'message' : 'texture',
                            'name' : name,
                            'width' : img.width,
                            'height' : img.height,
                            'data' : imageData.data.buffer});
  };
  img.src = name;
}

// Handle a message coming from the NaCl module.
function handleMessage(message_event) {
  if (message_event.data['message'] == 'benchmark_result') {
    // benchmark result
    var result = message_event.data['value'];
    console.log('Benchmark result:' + result);
    result = (Math.round(result * 1000) / 1000).toFixed(3);
    document.getElementById('result').textContent =
      'Result: ' + result + ' seconds';
    updateStatus('SUCCESS');
  } else if (message_event.data['message'] == 'set_zoom') {
    // zoom slider
    var zoom = message_event.data['value'];
    document.getElementById('zoomRange').value = zoom;
  } else if (message_event.data['message'] == 'set_light') {
    // light slider
    var light = message_event.data['value'];
    document.getElementById('lightRange').value = light;
  } else if (message_event.data['message'] == 'request_textures') {
    // NaCl module is requesting a set of textures.
    var names = message_event.data['names'];
    for (var i = 0; i < names.length; i++)
      loadTexture(names[i]);
  }
}

/**
 * Some elements are "auxiliary" to the NaCl embed area. They should only be
 * shown when the module actually loads. This function allows toggling their
 * visibility.
 *
 * @param {bool} isVisible The requested visibility.
 */
function setAuxElementsVisibility(isVisible) {
  var elems = ['config-demo', 'auxnotes'];
  var visibility = isVisible ? 'block' : 'none';

  for (var i = 0; i < elems.length; ++i) {
    var elem = document.getElementById(elems[i]);
    elem.style.display = visibility;
  }
}

// Listen for the DOM content to be loaded. This event is fired when parsing of
// the page's document has finished.
document.addEventListener('DOMContentLoaded', function() {
  var body = document.body;
  // The data-* attributes on the body can be referenced via body.dataset.
  if (body.dataset) {
    var attrs = {};
    if (body.dataset.attrs) {
      var attr_list = body.dataset.attrs.split(' ');
      for (var key in attr_list) {
        var attr = attr_list[key].split('=');
        var key = attr[0];
        var value = attr[1];
        attrs[key] = value;
      }
    }

    domContentLoaded(body.dataset.name,
                     body.dataset.width,
                     body.dataset.height, attrs);
  }
});
