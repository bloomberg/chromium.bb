// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) { window.console.log(msg); };
LOG('Guest script loading.');

// The window reference of the embedder to send post message reply.
var embedderWindowChannel = null;

// A value that uniquely identifies the guest sending the messages to the
// embedder.
var channelId = 0;
var notifyEmbedder = function(msg_array) {
  var msg = msg_array.concat([channelId]);
  embedderWindowChannel.postMessage(JSON.stringify(msg), '*');
};

var onPostMessageReceived = function(e) {
  embedderWindowChannel = e.source;
  var data = JSON.parse(e.data);
  if (data[0] == 'connect') {
    channelId = data[1];
    notifyEmbedder(['connected']);
    return;
  }
};
window.addEventListener('message', onPostMessageReceived, false);
