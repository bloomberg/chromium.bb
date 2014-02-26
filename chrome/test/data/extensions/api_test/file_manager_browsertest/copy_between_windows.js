// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

testcase.copyBetweenWindows = function() {
  var windowId1;
  var windowId2;
  StepsRunner.run([
    // Make a file in a local directory.
    function() {
      addEntries(['local'], [ENTRIES.hello], this.next);
    },
    // Open a new window.
    function(result) {
      chrome.test.assertTrue(result);
      openNewWindow(null, RootPath.DOWNLOADS, this.next);
    },
    // Open one more window.
    function(appId) {
      chrome.test.assertTrue(!!appId);
      windowId1 = appId;
      openNewWindow(null, RootPath.DRIVE, this.next);
    },
    // Wait for the file.
    function(appId) {
      chrome.test.assertTrue(!!appId);
      windowId2 = appId;
      callRemoteTestUtil('waitForFiles',
                         windowId1,
                         [[ENTRIES.hello.getExpectedRow()]],
                         this.next);
    },
    // Select the file.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('selectFile',
                         windowId1,
                         [ENTRIES.hello.nameText],
                         this.next);
    },
    // Copy it.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('execCommand', windowId1, ['copy'], this.next);
    },
    // Check the content of window 2 is empty.
    function(result) {
      callRemoteTestUtil('waitForFiles', windowId2, [[]], this.next);
    },
    // Paste it.
    function(result) {
      // Paste it.
      chrome.test.assertTrue(result);
      callRemoteTestUtil('execCommand', windowId2, ['paste'], this.next);
    },
    // Wait until the paste operation completes.
    function(result) {
      callRemoteTestUtil('waitForFiles',
                          windowId2,
                          [[ENTRIES.hello.getExpectedRow()],
                           {ignoreLastModifiedTime: true}],
                          this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
