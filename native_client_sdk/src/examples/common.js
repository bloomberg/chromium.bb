// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

naclModule = null;  // Global application object.
statusText = 'NO-STATUSES';

function extractSearchParameter(name, def_value) {
  var nameIndex = window.location.search.indexOf(name + "=");
  if (nameIndex != -1) {
    var value = location.search.substring(nameIndex + name.length + 1);
    var endIndex = value.indexOf("&");
    if (endIndex != -1)
      value = value.substring(0, endIndex);
    return value;
  }
  return def_value;
}

function createNaClModule(name, tool, width, height) {
  var listenerDiv = document.getElementById('listener');
  var naclModule = document.createElement('embed');
  naclModule.setAttribute('name', 'nacl_module');
  naclModule.setAttribute('id', 'nacl_module');
  naclModule.setAttribute('width', width);
  naclModule.setAttribute('height',height);
  naclModule.setAttribute('src', tool + '/' + name + '.nmf');
  naclModule.setAttribute('type', 'application/x-nacl');
  listenerDiv.appendChild(naclModule);
}

// Indicate success when the NaCl module has loaded.
function moduleDidLoad() {
  naclModule = document.getElementById('nacl_module');
  updateStatus('SUCCESS');
}

// Handle a message coming from the NaCl module.
function handleMessage(message_event) {
  alert(message_event.data);
}

// If the page loads before the Native Client module loads, then set the
// status message indicating that the module is still loading.  Otherwise,
// do not change the status message.
function pageDidLoad(name, tool, width, height) {
  updateStatus('Page loaded.');
  if (naclModule == null) {
    updateStatus('Creating embed: ' + tool)
    width = typeof width !== 'undefined' ? width : 200;
    height = typeof height !== 'undefined' ? height : 200;
    createNaClModule(name, tool, width, height)
  } else {
    // It's possible that the Native Client module onload event fired
    // before the page's onload event.  In this case, the status message
    // will reflect 'SUCCESS', but won't be displayed.  This call will
    // display the current message.
    updateStatus('Waiting.');
  }
}

// Set the global status message.  If the element with id 'statusField'
// exists, then set its HTML to the status message as well.
// opt_message The message test.  If this is null or undefined, then
//     attempt to set the element with id 'statusField' to the value of
//     |statusText|.
function updateStatus(opt_message) {
  if (opt_message) {
    statusText = opt_message; 
  }
  var statusField = document.getElementById('statusField');
  if (statusField) {
    statusField.innerHTML = statusText;
  }
}

