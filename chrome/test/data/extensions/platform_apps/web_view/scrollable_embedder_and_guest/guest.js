// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) {
  window.console.log(msg);
};

var embedder;

function sendMessageToEmbedder(message) {
  if (!embedder) {
    LOG('no embedder channel to send postMessage');
    return;
  }
  embedder.postMessage(JSON.stringify([message]), '*');
}

window.addEventListener('message', function(e) {
  embedder = e.source;
  var data = JSON.parse(e.data);
  if (data[0] == 'connect') {
    sendMessageToEmbedder('connected');
  }
});
