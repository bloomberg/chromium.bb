// Copyright 2014 The Chromium Authors. All rights reserved.
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
      waitForFileListChange(appId, BASIC_DRIVE_ENTRY_SET.length).
          then(this.next);
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
      waitForFileListChange(appId, BASIC_DRIVE_ENTRY_SET.length).
          then(this.next);
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
      waitForFileListChange(appId, BASIC_DRIVE_ENTRY_SET.length).
          then(this.next);
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
    '\'hello\' - search Drive',
    'hello.txt',
  ];
  var appId;

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next);
    },
    // Focus the search box.
    function(inAppId, list) {
      appId = inAppId;
      callRemoteTestUtil('fakeEvent',
                         appId,
                         ['#search-box input', 'focus'],
                         this.next);
    },
    // Input a text.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('inputText',
                         appId,
                         ['#search-box input', 'hello'],
                         this.next);
    },
    // Notify the element of the input.
    function() {
      callRemoteTestUtil('fakeEvent',
                         appId,
                         ['#search-box input', 'input'],
                         this.next);
    },
    // Wait for the auto complete list getting the expected contents.
    function(result) {
      chrome.test.assertTrue(result);
      repeatUntil(function() {
        return callRemoteTestUtil('queryAllElements',
                                  appId,
                                  ['#autocomplete-list li']).
            then(function(elements) {
              var list = elements.map(
                  function(element) { return element.text; });
              return chrome.test.checkDeepEq(EXPECTED_AUTOCOMPLETE_LIST, list) ?
                  undefined :
                  pending('Current auto complete list: %j.', list);
            });
      }).
      then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
