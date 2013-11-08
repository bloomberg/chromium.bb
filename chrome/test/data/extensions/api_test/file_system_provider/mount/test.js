// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function goodDisplayName() {
    chrome.fileSystemProvider.mount(
      'test file system',
      function(fileSystemId) {
        chrome.test.assertEq('string', typeof(fileSystemId));
        chrome.test.assertTrue(fileSystemId != '');
        chrome.test.succeed();
      },
      function(error) {
        chrome.test.fail();
      }
    );
  },
  function emptyDisplayName() {
    chrome.fileSystemProvider.mount(
      '',
      function(fileSystemId) {
        chrome.test.fail();
      },
      function(error) {
        chrome.test.assertEq('SecurityError', error.name);
        chrome.test.succeed();
      }
    );
  },
]);
