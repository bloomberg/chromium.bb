// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Check that there is no launch data.
function onLaunched(launchData) {
  chrome.test.runTests([
    function testIntent() {
      chrome.test.assertTrue(!launchData, "LaunchData found");
      chrome.test.succeed();
    }
  ]);
}

chrome.experimental.app.onLaunched.addListener(onLaunched);
