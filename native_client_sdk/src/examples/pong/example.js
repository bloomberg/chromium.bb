// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called by the common.js module.
function domContentLoaded(name, tc, config, width, height) {
  window.webkitStorageInfo.requestQuota(window.PERSISTENT, 1024,
      function(bytes) {
        common.updateStatus(
            'Allocated '+bytes+' bytes of persistent storage.');
        common.createNaClModule(name, tc, config, 800, 600);
        common.attachDefaultListeners();
      },
      function(e) { alert('Failed to allocate space'); });
}

// Called by the common.js module.
function attachListeners() {
  document.getElementById('resetScore').addEventListener('click', resetScore);
}

function resetScore() {
  common.naclModule.postMessage("resetScore");
}

// Handle a message coming from the NaCl module.  The message payload is
// assumed to contain the current game score.  Update the score text
// display with this value.
// Note that errors are also sent to this handler.  A message starting
// with 'ERROR' is considered an error, all other strings are assumed
// to be scores.
function handleMessage(message_event) {
  if (message_event.data.indexOf('ERROR') == 0) {
    document.getElementById('errorLog').innerHTML = message_event.data;
  } else {
    document.getElementById('score').innerHTML = message_event.data;
  }
}
