// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getWritableEntry() {
    chrome.fileSystem.chooseEntry({suggestedName: 'test.js'},
                                  chrome.test.callbackPass(function(entry) {
      chrome.test.assertEq('test.js', entry.name);
      // Test that we cannot get a writable entry when it's in a disallowed
      // location.
      chrome.fileSystem.getWritableEntry(entry, chrome.test.callbackFail(
          'Cannot write to file in a restricted location'));
    }));
  }
]);
