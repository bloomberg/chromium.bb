// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testCreation() {
    chrome.experimental.socket.create("udp", {},
      function callback(socketInfo) {
        chrome.test.assertEq(42, socketInfo.socketId);
        chrome.test.succeed();
      });
  }
]);
