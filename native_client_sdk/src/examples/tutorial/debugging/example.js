// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var lastModuleError = '';
var crashed = false;

function domContentLoaded(name, tc, config, width, height) {
  common.createNaClModule(name, tc, config, width, height);
  common.attachDefaultListeners();

  updateStatus('Page Loaded');
  heartBeat();
}

// Indicate success when the NaCl module has loaded.
function moduleDidLoad() {
  updateStatus('LOADED');
  setTimeout(boom, 4000);
}

// Handle a message coming from the NaCl module.
function handleMessage(message_event) {
  msg_type = message_event.data.substring(0, 4);
  msg_data = message_event.data.substring(5, message_event.data.length);
  if (msg_type == 'POP:') {
    alert(message_event.data);
    return;
  }
  if (msg_type == 'LOG:') {
    document.getElementById('log').value += msg_data + '\n';
    return;
  }
  if (msg_type == 'TRC:') {
    crashed = true;
    updateStatus('Crash Reported');
    xmlhttp = new XMLHttpRequest();
    xmlhttp.open('POST', common.naclModule.src, false);
    xmlhttp.send(msg_data);
    document.getElementById('trace').value = xmlhttp.responseText + '\n';
    return;
  }
}

function updateStatus(message) {
  common.updateStatus(message);

  if (message)
    document.getElementById('log').value += message + '\n'
}

function boom() {
  if (!crashed) {
    updateStatus('Send BOOM');
    common.naclModule.postMessage('BOOM');
    setTimeout(boom, 1000);
  }
}

function heartBeat() {
  if (common.naclModule && common.naclModule.lastError) {
    if (lastModuleError != common.naclModule.lastError) {
      lastModuleError = common.naclModule.lastError;
      updateStatus('Missed heartbeat: ' + common.naclModule.lastError);
      crashed = true;
    }
  } else {
    setTimeout(heartBeat, 1000);
  }
}
