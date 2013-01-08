// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testId = "bjafgdebaacbbbecmhlhpofkepfkgcpa";

// Call with |api| as either chrome.runtime or chrome.extension, so that both
// get tested (extension is aliased to runtime).
function connectExternalTest(api) {
  var port = api.connect(testId, {name: "extern"});
  port.postMessage({testConnectExternal: true});
  port.onMessage.addListener(chrome.test.callbackPass(function(msg) {
    chrome.test.assertTrue(msg.success, "Message failed.");
    chrome.test.assertEq(msg.senderId, location.host,
                         "Sender ID doesn't match.");
  }));
}

chrome.test.runTests([
  function connectExternal_extension() {
    connectExternalTest(chrome.extension);
  },
  function connectExternal_runtime() {
    connectExternalTest(chrome.runtime);
  }
]);
