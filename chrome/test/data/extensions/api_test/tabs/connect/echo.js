// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content script that echoes back all messages.
// Posting a message with "GET" returns the name and # of connections opened.
var connections = 0;

chrome.runtime.onConnect.addListener(function onConnect(port) {
  connections++;
  port.onMessage.addListener(function onMessage(msg) {
    if (msg == "GET") {
      port.postMessage({"name": port.name, "connections": connections});
    } else {
      port.postMessage(msg);
    }
  });
});

chrome.extension.onRequest.addListener(function(request, sender, respond) {
  respond(request);
});
