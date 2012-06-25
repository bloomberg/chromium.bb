// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getDisplayPath() {
    chrome.fileSystem.chooseFile(chrome.test.callbackPass(function(entry) {
      chrome.test.assertEq('open_existing.txt', entry.name);
      // Test that we can get the display path of the file.
      chrome.fileSystem.getDisplayPath(entry, chrome.test.callbackPass(
          function(path) {
        chrome.test.assertTrue(path.indexOf("file_system") >= 0);
        chrome.test.assertTrue(path.indexOf("open_existing.txt") >= 0);
      }));
    }));
  }
]);
