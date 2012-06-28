// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getWritableEntry() {
    chrome.fileSystem.chooseFile(chrome.test.callbackPass(function(entry) {
      chrome.test.assertEq('writable.txt', entry.name);
      // Test that we can get the display path of the file.
      chrome.fileSystem.getWritableFileEntry(entry, chrome.test.callbackPass(
          function(writable) {
        checkEntry(writable, 'writable.txt', false, true);
      }));
    }));
  }
]);
