// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The test extension for externally_connectable just echos all messages it
// gets. The test runners are in the sites/ directory.

chrome.runtime.onMessageExternal.addListener(function(message, sender, reply) {
  reply({ message: message, sender: sender });
});

chrome.runtime.onConnectExternal.addListener(function(port) {
  port.onMessage.addListener(function(message) {
    port.postMessage({ message: message, sender: port.sender });
  });
});
