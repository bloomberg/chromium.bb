// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getEntryIdWorks() {
    chrome.fileSystem.chooseEntry({type: 'openFile'},
        chrome.test.callbackPass(function(entry) {
      var id = chrome.fileSystem.getEntryId(entry);
      chrome.test.assertTrue(id != null);
      var e = chrome.fileSystem.getEntryById(id);
      chrome.test.assertEq(e, entry);
    }));
  }
]);
