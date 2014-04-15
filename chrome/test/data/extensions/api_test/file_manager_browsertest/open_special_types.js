// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests if the gallery shows up for the selected image and that the image
 * gets displayed.
 *
 * @param {string} path Directory path to be tested.
 */
function galleryOpen(path) {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Resize the window to desired dimensions to avoid flakyness.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil('resizeWindow',
                         appId,
                         [480, 480],
                         this.next);
    },
    // Select the image.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('openFile',
                         appId,
                         ['My Desktop Background.png'],
                         this.next);
    },
    // Wait for the image in the gallery's screen image.
    function(result) {
      chrome.test.assertTrue(result);
      waitForElement(appId,
                     '.gallery .content canvas.image',
                     'iframe.overlay-pane').then(this.next);
    },
    // Verify the gallery's screen image.
    function(element) {
      chrome.test.assertEq('480', element.attributes.width);
      chrome.test.assertEq('360', element.attributes.height);
      // Get the full-resolution image.
      waitForElement(appId,
                         '.gallery .content canvas.fullres',
                         'iframe.overlay-pane').then(this.next);
    },
    // Verify the gallery's full resolution image.
    function(element) {
      chrome.test.assertEq('800', element.attributes.width);
      chrome.test.assertEq('600', element.attributes.height);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

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

testcase.galleryOpenDownloads = function() {
  galleryOpen(RootPath.DOWNLOADS);
};

testcase.galleryOpenDrive = function() {
  galleryOpen(RootPath.DRIVE);
};

testcase.zipOpenDownloads = function() {
  zipOpen(RootPath.DOWNLOADS);
};

testcase.zipOpenDrive = function() {
  zipOpen(RootPath.DRIVE);
};
