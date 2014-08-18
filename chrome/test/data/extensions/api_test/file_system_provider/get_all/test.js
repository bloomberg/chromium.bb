// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Runs all of the test cases, one by one.
 */
chrome.test.runTests([
  // Verifies if getAll() returns the mounted file system.
  function mountSuccess() {
    var onTestSuccess = chrome.test.callbackPass();
    chrome.fileSystemProvider.mount(
        {
          fileSystemId: test_util.FILE_SYSTEM_ID,
          displayName: test_util.FILE_SYSTEM_NAME
        },
        function() {
          chrome.fileSystemProvider.getAll(function(fileSystems) {
            chrome.test.assertEq(1, fileSystems.length);
            chrome.test.assertEq(
                test_util.FILE_SYSTEM_ID, fileSystems[0].fileSystemId);
            chrome.test.assertEq(
                test_util.FILE_SYSTEM_NAME, fileSystems[0].displayName);
            chrome.test.assertFalse(fileSystems[0].writable);
            onTestSuccess();
          });
        },
        function(error) {
          chrome.test.fail(error.name);
        });
  },

  // Verifies that after unmounting, the file system is not available in
  // getAll() list.
  function unmountSuccess() {
    var onTestSuccess = chrome.test.callbackPass();
    chrome.fileSystemProvider.unmount(
        {fileSystemId: test_util.FILE_SYSTEM_ID},
        function() {
          chrome.fileSystemProvider.getAll(function(fileSystems) {
            chrome.test.assertEq(0, fileSystems.length);
          });
          onTestSuccess();
        },
        function(error) {
          chrome.test.fail(error.name);
        });
  },

  // Verifies that if mounting fails, then the file system is not added to the
  // getAll() list.
  function mountError() {
    var onTestSuccess = chrome.test.callbackPass();
    chrome.fileSystemProvider.mount(
        {fileSystemId: '', displayName: ''},
        function() {
          chrome.test.fail();
        },
        function(error) {
          chrome.test.assertEq('SecurityError', error.name);
          chrome.fileSystemProvider.getAll(function(fileSystems) {
            chrome.test.assertEq(0, fileSystems.length);
          });
          onTestSuccess();
        }
      );
  }
]);
