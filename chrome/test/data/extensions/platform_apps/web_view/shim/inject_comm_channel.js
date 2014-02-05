// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
// This will be overriden in specific tests.
embedder.processMessage = function(data) {
  return false;
};

function reportConnected() {
  var msg = ['connected'];
  embedder.channel.postMessage(JSON.stringify(msg), '*');
}

function reportError(messageType) {
  var msg = ['error', messageType];
  embedder.channel.postMessage(JSON.stringify(msg), '*');
}

window.addEventListener('message', function(e) {
  embedder.channel = e.source;
  var data = JSON.parse(e.data);
  if (data[0] == 'connect') {
    reportConnected();
    return;
  }
  if (!embedder.processMessage(data)) {
    reportError(data[0]);
    return;
  }
});

