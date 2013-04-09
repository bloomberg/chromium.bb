// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called by the common.js module.
function moduleDidLoad() {
  // The module is not hidden by default so we can easily see if the plugin
  // failed to load.
  common.hideModule();
}

// Called by the common.js module.
function attachListeners() {
  document.getElementById('fortyTwo').addEventListener('click', fortyTwo);
  document.getElementById('reverseText').addEventListener('click', reverseText);
}

function fortyTwo() {
  common.naclModule.postMessage('fortyTwo');
}

function reverseText() {
  // Grab the text from the text box, pass it into reverseText()
  var inputBox = document.getElementById('inputBox');
  common.naclModule.postMessage('reverseText:' + inputBox.value);
}

function handleMessage(e) {
  if (typeof e.data === 'string') {
    // Received a reversed message.
    common.logMessage('Received "' + e.data + '"\n');
  } else if (typeof e.data === 'number') {
    // Recived 42.
    common.logMessage('Received "' + e.data + '"\n');
  }
}
