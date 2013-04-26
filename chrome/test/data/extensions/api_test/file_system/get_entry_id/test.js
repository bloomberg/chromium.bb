// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getEntryIdWorks() {
    chrome.runtime.getBackgroundPage(
        chrome.test.callbackPass(function(backgroundPage) {
      backgroundPage.callback = chrome.test.callbackPass(
          function(other_window, id, entry) {
        other_window.close();
        chrome.test.assertEq(chrome.fileSystem.getEntryId(entry), id);
        chrome.test.assertEq(chrome.fileSystem.getEntryById(id), entry);
        chrome.test.assertEq(
            chrome.fileSystem.getEntryId(chrome.fileSystem.getEntryById(id)),
            id);
        chrome.test.assertEq(
            chrome.fileSystem.getEntryById(chrome.fileSystem.getEntryId(entry)),
            entry);
        checkEntry(entry, 'writable.txt', false /* isNew */,
          false /*shouldBeWritable */);
      });
      chrome.app.window.create('test_other_window.html');
    }));
  }
]);
