// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Test function to copy from the specified source to the specified destination.
 * @param {string} targetFile Name of target file to be copied.
 * @param {string} srcName Type of source volume. e.g. downloads, drive,
 *     drive_recent, drive_shared_with_me, drive_offline.
 * @param {Array.<TestEntryInfo>} srcEntries Expected initial contents in the
 *     source volume.
 * @param {string} dstName Type of destination volume.
 * @param {Array.<TestEntryInfo>} dstEntries Expected initial contents in the
 *     destination volume.
 */
function copyBetweenVolumes(targetFile,
                            srcName,
                            srcEntries,
                            dstName,
                            dstEntries) {
  var srcContents = TestEntryInfo.getExpectedRows(srcEntries).sort();
  var dstContents = TestEntryInfo.getExpectedRows(dstEntries).sort();

  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Select the source volume.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil(
          'selectVolume', appId, [srcName], this.next);
    },
    // Wait for the expected files to appear in the file list.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil(
          'waitForFiles', appId, [srcContents], this.next);
    },
    // Select the source file.
    function() {
      callRemoteTestUtil(
          'selectFile', appId, [targetFile], this.next);
    },
    // Copy the file.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('execCommand', appId, ['copy'], this.next);
    },
    // Select the destination volume.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil(
          'selectVolume', appId, [dstName], this.next);
    },
    // Wait for the expected files to appear in the file list.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil(
          'waitForFiles', appId, [dstContents], this.next);
    },
    // Paste the file.
    function() {
      callRemoteTestUtil(
          'execCommand', appId, ['paste'], this.next);
    },
    // Wait for the file list to change.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForFileListChange',
                         appId,
                         [dstContents.length],
                         this.next);
    },
    // Check the last contents of file list.
    function(actualFilesAfter) {
      chrome.test.assertEq(dstContents.length + 1,
                           actualFilesAfter.length);
      var copiedItem = null;
      for (var i = 0; i < srcContents.length; i++) {
        if (srcContents[i][0] == targetFile) {
          copiedItem = srcContents[i];
          break;
        }
      }
      chrome.test.assertTrue(copiedItem != null);
      for (var i = 0; i < dstContents.length; i++) {
        if (dstContents[i][0] == targetFile) {
          // Replace the last '.' in filename with ' (1).'.
          // e.g. 'my.note.txt' -> 'my.note (1).txt'
          copiedItem[0] = copiedItem[0].replace(/\.(?=[^\.]+$)/, ' (1).');
          break;
        }
      }
      // File size can not be obtained on drive_shared_with_me volume and
      // drive_offline.
      var ignoreSize = srcName == 'drive_shared_with_me' ||
                       dstName == 'drive_shared_with_me' ||
                       srcName == 'drive_offline' ||
                       dstName == 'drive_offline';
      for (var i = 0; i < actualFilesAfter.length; i++) {
        if (actualFilesAfter[i][0] == copiedItem[0] &&
            (ignoreSize || actualFilesAfter[i][1] == copiedItem[1]) &&
            actualFilesAfter[i][2] == copiedItem[2]) {
          checkIfNoErrorsOccured(this.next);
          return;
        }
      }
      chrome.test.fail();
    }
  ]);
}

/**
 * Tests copy from drive's root to local's downloads.
 */
testcase.transferFromDriveToDownloads = copyBetweenVolumes.bind(
    null,
    'hello.txt',
    'drive',
    BASIC_DRIVE_ENTRY_SET,
    'downloads',
    BASIC_LOCAL_ENTRY_SET);

/**
 * Tests copy from local's downloads to drive's root.
 */
testcase.transferFromDownloadsToDrive = copyBetweenVolumes.bind(
    null,
    'hello.txt',
    'downloads',
    BASIC_LOCAL_ENTRY_SET,
    'drive',
    BASIC_DRIVE_ENTRY_SET);

/**
 * Tests copy from drive's shared_with_me to local's downloads.
 */
testcase.transferFromSharedToDownloads = copyBetweenVolumes.bind(
    null,
    'Test Shared Document.gdoc',
    'drive_shared_with_me',
    SHARED_WITH_ME_ENTRY_SET,
    'downloads',
    BASIC_LOCAL_ENTRY_SET);

/**
 * Tests copy from drive's shared_with_me to drive's root.
 */
testcase.transferFromSharedToDrive = copyBetweenVolumes.bind(
    null,
    'Test Shared Document.gdoc',
    'drive_shared_with_me',
    SHARED_WITH_ME_ENTRY_SET,
    'drive',
    BASIC_DRIVE_ENTRY_SET);

/**
 * Tests copy from drive's recent to local's downloads.
 */
testcase.transferFromRecentToDownloads = copyBetweenVolumes.bind(
    null,
    'hello.txt',
    'drive_recent',
    RECENT_ENTRY_SET,
    'downloads',
    BASIC_LOCAL_ENTRY_SET);

/**
 * Tests copy from drive's recent to drive's root.
 */
testcase.transferFromRecentToDrive = copyBetweenVolumes.bind(
    null,
    'hello.txt',
    'drive_recent',
    RECENT_ENTRY_SET,
    'drive',
    BASIC_DRIVE_ENTRY_SET);

/**
 * Tests copy from drive's offline to local's downloads.
 */
testcase.transferFromOfflineToDownloads = copyBetweenVolumes.bind(
    null,
    'Test Document.gdoc',
    'drive_offline',
    OFFLINE_ENTRY_SET,
    'downloads',
    BASIC_LOCAL_ENTRY_SET);

/**
 * Tests copy from drive's offline to drive's root.
 */
testcase.transferFromOfflineToDrive = copyBetweenVolumes.bind(
    null,
    'Test Document.gdoc',
    'drive_offline',
    OFFLINE_ENTRY_SET,
    'drive',
    BASIC_DRIVE_ENTRY_SET);
