// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testCreation() {
    chrome.experimental.socket.create("udp", {},
      function callback(socketInfo) {
        chrome.test.assertTrue(socketInfo.socketId > 0);
        chrome.test.succeed();
      });
  }
]);
