// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function isWritableEntry() {
    chrome.fileSystem.chooseFile({type: 'saveFile'},
        chrome.test.callbackPass(function(entry) {
      chrome.test.assertEq('writable.txt', entry.name);
      // Test that the file is writable, as we requested a 'saveFile'.
      chrome.fileSystem.isWritableFileEntry(entry, chrome.test.callbackPass(
          function(isWritable) {
        chrome.test.assertTrue(isWritable);
      }));
    }));
  },
  function isNotWritableEntry() {
    chrome.fileSystem.chooseFile(chrome.test.callbackPass(function(entry) {
      chrome.test.assertEq('writable.txt', entry.name);
      // The file should not be writable, since the default is 'openFile'.
      chrome.fileSystem.isWritableFileEntry(entry, chrome.test.callbackPass(
          function(isWritable) {
        chrome.test.assertFalse(isWritable);
      }));
    }));
  }
]);
