// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests if the files initially added by the C++ side are displayed, and
 * that a subsequently added file shows up.
 *
 * @param {string} path Directory path to be tested.
 */
function fileDisplay(path) {
  var appId;

  var expectedFilesBefore =
      TestEntryInfo.getExpectedRows(path == RootPath.DRIVE ?
          BASIC_DRIVE_ENTRY_SET : BASIC_LOCAL_ENTRY_SET).sort();

  var expectedFilesAfter =
      expectedFilesBefore.concat([ENTRIES.newlyAdded.getExpectedRow()]).sort();

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Notify that the list has been verified and a new file can be added
    // in file_manager_browsertest.cc.
    function(inAppId, actualFilesBefore) {
      appId = inAppId;
      chrome.test.assertEq(expectedFilesBefore, actualFilesBefore);
      addEntries(['local', 'drive'], [ENTRIES.newlyAdded], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      waitForFileListChange(appId, expectedFilesBefore.length).then(this.next);
    },
    // Confirm the file list.
    function(actualFilesAfter) {
      chrome.test.assertEq(expectedFilesAfter, actualFilesAfter);
      checkIfNoErrorsOccured(this.next);
    },
  ]);
}

testcase.fileDisplayDownloads = function() {
  fileDisplay(RootPath.DOWNLOADS);
};

testcase.fileDisplayDrive = function() {
  fileDisplay(RootPath.DRIVE);
};
