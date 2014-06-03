// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests keyboard operations of the navigation list.
 */
testcase.traverseNavigationList = function() {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next);
    },
    // Wait until Google Drive is selected.
    function(inAppId) {
      appId = inAppId;
      waitForElement(
          appId,
          '#navigation-list > .root-item > ' +
               '.volume-icon[volume-type-icon="drive"]').then(this.next);
    },
    // Ensure that the current directory is changed to Google Drive.
    function() {
      waitForFiles(appId,
                   TestEntryInfo.getExpectedRows(BASIC_DRIVE_ENTRY_SET),
                   {ignoreLastModifiedTime: true}).then(this.next);
    },
    // Press the UP key.
    function() {
      callRemoteTestUtil('fakeKeyDown',
                         appId,
                         ['#navigation-list', 'Up', true],
                         this.next);
    },
    // Ensure that Gogole Drive is selected since it is the first item.
    function(result) {
      chrome.test.assertTrue(result);
      waitForElement(
          appId,
          '#navigation-list > .root-item > ' +
               '.volume-icon[volume-type-icon="drive"]').then(this.next);
    },
    // Press the DOWN key.
    function() {
      callRemoteTestUtil('fakeKeyDown',
                         appId,
                         ['#navigation-list', 'Down', true],
                         this.next);
    },
    // Ensure that Downloads is selected.
    function(result) {
      chrome.test.assertTrue(result);
      waitForElement(
          appId,
          '#navigation-list > .root-item > ' +
               '.volume-icon[volume-type-icon="downloads"]').then(this.next);
    },
    // Ensure that the current directory is changed to Downloads.
    function() {
      waitForFiles(
          appId,
          TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET),
          {ignoreLastModifiedTime: true}).then(this.next);
    },
    // Press the DOWN key again.
    function() {
      callRemoteTestUtil('fakeKeyDown',
                         appId,
                         ['#navigation-list', 'Down', true],
                         this.next);
    },
    // Ensure that Downloads is still selected since it is the last item.
    function(result) {
      chrome.test.assertTrue(result);
      waitForElement(
          appId,
          '#navigation-list > .root-item > ' +
               '.volume-icon[volume-type-icon="downloads"]').then(this.next);
    },
    // Check for errors.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
