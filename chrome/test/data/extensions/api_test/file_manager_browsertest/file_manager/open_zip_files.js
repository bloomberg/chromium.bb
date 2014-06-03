// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests if we can open and unmount a zip file.
 * @param {string} path Directory path to be tested.
 */
function zipOpen(path) {
  var appId;
  StepsRunner.run([
    // Add a ZIP file.
    function() {
      addEntries(['local', 'drive'], [ENTRIES.zipArchive], this.next);
    },
    // Open a window.
    function(result) {
      chrome.test.assertTrue(result);
      openNewWindow(null, path, this.next);
    },
    // Wait for going back.
    function(inAppId) {
      appId = inAppId;
      waitForElement(appId, '#detail-table').then(this.next);
    },
    function() {
      waitForFiles(appId, [ENTRIES.zipArchive.getExpectedRow()]).
          then(this.next);
    },
    // Open a file.
    function(result) {
      callRemoteTestUtil('openFile',
                         appId,
                         [ENTRIES.zipArchive.nameText],
                         this.next);
    },
    // Wait for ZIP contents.
    function(result) {
      chrome.test.assertTrue(result);
      waitForFiles(appId,
                   [[
                     'SUCCESSFULLY_PERFORMED_FAKE_MOUNT.txt',
                     '21 bytes',
                     'Plain text',
                     ''
                   ]],
                   {ignoreLastModifiedTime: true}).then(this.next);
    },
    // Unmount the zip.
    function(result) {
      waitForElement(appId, '.root-eject', this.next).then(this.next);
    },
    // Unmount the zip.
    function(element) {
      callRemoteTestUtil('fakeMouseClick',
                         appId,
                         ['.root-eject'],
                         this.next);
    },
    // Wait for going back.
    function(result) {
      chrome.test.assertTrue(result);
      waitForFiles(appId, [ENTRIES.zipArchive.getExpectedRow()]).
          then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

testcase.zipOpenDownloads = function() {
  zipOpen(RootPath.DOWNLOADS);
};

testcase.zipOpenDrive = function() {
  zipOpen(RootPath.DRIVE);
};
