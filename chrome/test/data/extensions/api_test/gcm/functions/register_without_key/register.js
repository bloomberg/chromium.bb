// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testRegister() {
    var senderIds = ["Sender1", "Sender2"];
    chrome.gcm.register(senderIds, function(registrationId) {
      if (chrome.runtime.lastError.message == "Manifest key was missing.")
        chrome.test.succeed();
      else
        chrome.test.fail("gcm.register should fail.");
    });
  }
]);
