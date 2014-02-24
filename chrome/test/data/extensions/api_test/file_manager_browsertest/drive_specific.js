// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests opening the "Recent" on the sidebar navigation by clicking the icon,
 * and verifies the directory contents. We test if there are only files, since
 * directories are not allowed in "Recent". This test is only available for
 * Drive.
 */
testcase.openSidebarRecent = function() {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next);
    },
    // Click the icon of the Recent volume.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil(
        'selectVolume', appId, ['drive_recent'], this.next);
    },
    // Wait until the file list is updated.
    function(result) {
      chrome.test.assertFalse(!result);
      callRemoteTestUtil(
          'waitForFileListChange',
          appId,
          [BASIC_DRIVE_ENTRY_SET.length],
          this.next);
    },
    // Verify the file list.
    function(actualFilesAfter) {
      chrome.test.assertEq(
          TestEntryInfo.getExpectedRows(RECENT_ENTRY_SET).sort(),
          actualFilesAfter);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests opening the "Offline" on the sidebar navigation by clicking the icon,
 * and checks contenets of the file list. Only the entries "available offline"
 * should be shown. "Available offline" entires are hosted documents and the
 * entries cached by DriveCache.
 */
testcase.openSidebarOffline = function() {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next);
    },
    // Click the icon of the Offline volume.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil(
        'selectVolume', appId, ['drive_offline'], this.next);
    },
    // Wait until the file list is updated.
    function(result) {
      chrome.test.assertFalse(!result);
      callRemoteTestUtil(
          'waitForFileListChange',
          appId,
          [BASIC_DRIVE_ENTRY_SET.length],
          this.next);
    },
    // Verify the file list.
    function(actualFilesAfter) {
      chrome.test.assertEq(
          TestEntryInfo.getExpectedRows(OFFLINE_ENTRY_SET).sort(),
          actualFilesAfter);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests opening the "Shared with me" on the sidebar navigation by clicking the
 * icon, and checks contents of the file list. Only the entries labeled with
 * "shared-with-me" should be shown.
 */
testcase.openSidebarSharedWithMe = function() {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next);
    },
    // Click the icon of the Shared With Me volume.
    function(inAppId) {
      appId = inAppId;
      // Use the icon for a click target.
      callRemoteTestUtil('selectVolume',
                         appId,
                         ['drive_shared_with_me'], this.next);
    },
    // Wait until the file list is updated.
    function(result) {
      chrome.test.assertFalse(!result);
      callRemoteTestUtil(
          'waitForFileListChange',
          appId,
          [BASIC_DRIVE_ENTRY_SET.length],
          this.next);
    },
    // Verify the file list.
    function(actualFilesAfter) {
      chrome.test.assertEq(
          TestEntryInfo.getExpectedRows(SHARED_WITH_ME_ENTRY_SET).sort(),
          actualFilesAfter);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests autocomplete with a query 'hello'. This test is only available for
 * Drive.
 */
testcase.autocomplete = function() {
  var EXPECTED_AUTOCOMPLETE_LIST = [
    '\'hello\' - search Drive\n',
    'hello.txt\n',
  ];

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next);
    },
    // Perform an auto complete test and wait until the list changes.
    // TODO(mtomasz): Move the operation from test_util.js to tests_cases.js.
    function(appId, list) {
      callRemoteTestUtil('performAutocompleteAndWait',
                         appId,
                         ['hello', EXPECTED_AUTOCOMPLETE_LIST.length],
                         this.next);
    },
    // Verify the list contents.
    function(autocompleteList) {
      chrome.test.assertEq(EXPECTED_AUTOCOMPLETE_LIST, autocompleteList);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
