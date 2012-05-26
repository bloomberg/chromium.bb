// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that there is a launchData.intent, it is set up proerly, and that the
// FileEntry in launchData.intent.data can be read.
function onLaunched(launchData) {
  chrome.test.runTests([
    function testIntent() {
      chrome.test.assertFalse(!launchData, "No launchData");
      chrome.test.assertFalse(!launchData.intent, "No launchData.intent");
      chrome.test.assertEq(launchData.intent.action,
          "http://webintents.org/view");
      chrome.test.assertEq(launchData.intent.type,
          "chrome-extension://fileentry");
      chrome.test.assertFalse(!launchData.intent.data,
          "No launchData.intent.data");

      launchData.intent.data.file(function(file) {
        var reader = new FileReader();
        reader.onloadend = function(e) {
          chrome.test.assertEq(
              reader.result.indexOf("This is a test. Word."), 0);
          chrome.test.succeed();
        };
        reader.onerror = function(e) {
          chrome.test.fail("Error reading file contents.");
        };
        reader.readAsText(file);
      });
    }
  ]);
}

chrome.experimental.app.onLaunched.addListener(onLaunched);
