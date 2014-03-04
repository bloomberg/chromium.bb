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
    // Open windows.
    function(result) {
      chrome.test.assertTrue(result);
      Promise.all([
        openNewWindow(null, RootPath.DOWNLOADS),
        openNewWindow(null, RootPath.DRIVE)
      ]).
      then(this.next);
    },
    // Wait loading documents in the windows.
    function(appIds) {
      windowId1 = appIds[0];
      windowId2 = appIds[1];
      Promise.all([
        waitForElement(windowId1, '#detail-table'),
        waitForElement(windowId2, '#detail-table'),
      ]).
      then(this.next);
    },
    // Wait for the file.
    function(appId) {
      waitForFiles(windowId1, [ENTRIES.hello.getExpectedRow()]).then(this.next);
    },
    // Select the file.
    function() {
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
      chrome.test.assertTrue(result);
      waitForFiles(windowId2, []).then(this.next);
    },
    // Paste it.
    function() {
      // Paste it.
      callRemoteTestUtil('execCommand', windowId2, ['paste'], this.next);
    },
    // Wait until the paste operation completes.
    function(result) {
      waitForFiles(windowId2,
                   [ENTRIES.hello.getExpectedRow()],
                   {ignoreLastModifiedTime: true}).
          then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
