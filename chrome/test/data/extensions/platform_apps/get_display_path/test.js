// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function (launchData) {
  // Test that there is launchData, it is set up properly, and that the
  // FileEntry in launchData.items[0].entry can have its display path gotten.
  chrome.test.runTests([
    function testGetDisplayPath() {
      chrome.test.assertFalse(!launchData, "No launchData");
      chrome.test.assertFalse(!launchData.items[0], "No launchData.items[0]");
      chrome.test.assertFalse(!launchData.items[0].entry,
                              "No launchData.items[0].entry");
      var entry = launchData.items[0].entry;
      chrome.fileSystem.getDisplayPath(entry,
          chrome.test.callbackPass(function(path) {
        chrome.test.assertFalse(path.indexOf('test.txt') == -1);
      }));
    }
  ]);
});
