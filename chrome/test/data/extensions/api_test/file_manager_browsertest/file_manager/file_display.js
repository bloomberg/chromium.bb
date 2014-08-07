// Copyright 2014 The Chromium Authors. All rights reserved.
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

testcase.fileDisplayMtp = function() {
  var appId;
  var MTP_VOLUME_QUERY = '#directory-tree > .tree-item > .tree-row > ' +
    '.volume-icon[volume-type-icon="mtp"]';

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Mount a fake MTP volume.
    function(inAppId, files) {
      appId = inAppId;
      chrome.test.sendMessage(JSON.stringify({name: 'mountFakeMtp'}),
                              this.next);
    },
    // Wait for the mount.
    function(result) {
      chrome.test.assertTrue(JSON.parse(result));
      waitForElement(appId, MTP_VOLUME_QUERY).then(this.next);
    },
    // Click the MTP volume.
    function() {
      callRemoteTestUtil(
          'fakeMouseClick', appId, [MTP_VOLUME_QUERY], this.next);
    },
    // Wait for the file list to change.
    function(appIds) {
      waitForFiles(appId, TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET),
                   {ignoreLastModifiedTime: true}).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
