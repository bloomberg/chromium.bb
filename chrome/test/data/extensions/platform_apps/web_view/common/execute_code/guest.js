// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.console.log('Guest script loading.');

// The window reference of the embedder to send post message reply.
var embedderWindowChannel = null;

var notifyEmbedder = function(msg_array) {
  embedderWindowChannel.postMessage(JSON.stringify(msg_array), '*');
};

var onPostMessageReceived = function(e) {
  embedderWindowChannel = e.source;
  var data = JSON.parse(e.data);
  if (data[0] == 'create-channel') {
    notifyEmbedder(['channel-created']);
    return;
  }
  if (data[0] == 'get-style') {
    var propertyName = data[1];
    var computedStyle = window.getComputedStyle(document.body, null);
    var value = computedStyle.getPropertyValue(propertyName);
    notifyEmbedder(['style', propertyName, value]);
    return;
  }
};

window.addEventListener('message', onPostMessageReceived, false);
