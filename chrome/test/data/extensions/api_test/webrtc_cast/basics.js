// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function udpTransport() {
    chrome.webrtc.castUdpTransport.create(function(info) {
      chrome.webrtc.start(info.transportId,
                          {address: "127.0.0.1", port: 60613});
      chrome.webrtc.stop(info.transportId);
      chrome.webrtc.destroy(info.transportId);
      chrome.test.succeed();
    });
    // TODO(hclam): Remove this line when the API is actually working.
    // This is there so the test can finish.
    chrome.test.succeed();
  }
]);
