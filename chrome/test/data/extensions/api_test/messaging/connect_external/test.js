// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testId = "bjafgdebaacbbbecmhlhpofkepfkgcpa";

chrome.test.runTests([
  function connectExternal() {
    var port = chrome.extension.connect(testId, {name: "extern"});
    port.postMessage({testConnectExternal: true});
    port.onMessage.addListener(chrome.test.callbackPass(function(msg) {
      chrome.test.assertTrue(msg.success, "Message failed.");
      chrome.test.assertEq(msg.senderId, location.host,
                           "Sender ID doesn't match.");
    }));
  }
]);
