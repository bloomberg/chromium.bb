// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function(launchData) {
  // Test that there is a launchData.intent, and it is set up properly. Then
  // immediately reply to the intent with a success message.
  chrome.test.runTests([
    function testIntent() {
      chrome.test.assertFalse(!launchData, "No launchData");
      chrome.test.assertFalse(!launchData.intent, "No launchData.intent");
      launchData.intent.postResult(true);
    }
  ]);
});
