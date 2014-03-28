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
                         [320, 320],
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
      chrome.test.assertEq('320', element.attributes.width);
      chrome.test.assertEq('240', element.attributes.height);
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
 * Tests if the audio player shows up for the selected image and that the audio
 * is loaded successfully.
 *
 * @param {string} path Directory path to be tested.
 */
function audioOpen(path) {
  var appId;
  var audioAppId;

  var expectedFilesBefore =
      TestEntryInfo.getExpectedRows(path == RootPath.DRIVE ?
          BASIC_DRIVE_ENTRY_SET : BASIC_LOCAL_ENTRY_SET).sort();
  var expectedFilesAfter =
      expectedFilesBefore.concat([ENTRIES.newlyAdded.getExpectedRow()]).sort();

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Select the song.
    function(inAppId) {
      appId = inAppId;

      // Add an additional audio file.
      addEntries(['local', 'drive'], [ENTRIES.newlyAdded], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      waitForFileListChange(appId, expectedFilesBefore.length).then(this.next);
    },
    function(actualFilesAfter) {
      chrome.test.assertEq(expectedFilesAfter, actualFilesAfter);
      callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the audio player window.
    function(result) {
      chrome.test.assertTrue(result);
      waitForWindow('audio_player.html').then(this.next);
    },
    // Wait for the changes of the player status.
    function(inAppId) {
      audioAppId = inAppId;
      waitForElement(audioAppId, 'audio-player[playing]').then(this.next);
    },
    // Get the source file name.
    function(element) {
      chrome.test.assertEq(
          'filesystem:chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/' +
              'external' + path + '/Beautiful%20Song.ogg',
          element.attributes.currenttrackurl);

      // Open another file.
      callRemoteTestUtil(
          'openFile', appId, ['newly added file.ogg'], this.next);
    },
    // Wait for the changes of the player status.
    function(result) {
      chrome.test.assertTrue(result);
      var query = 'audio-player' +
                  '[playing]' +
                  '[currenttrackurl$="newly%20added%20file.ogg"]';
      waitForElement(audioAppId, query).then(this.next);
    },
    // Get the source file name.
    function(element) {
      chrome.test.assertEq(
          'filesystem:chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/' +
              'external' + path + '/newly%20added%20file.ogg',
          element.attributes.currenttrackurl);

      // Close window
      closeWindowAndWait(audioAppId).then(this.next);
    },
    // Wait for the audio player.
    function(result) {
      chrome.test.assertTrue(result);
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

testcase.audioOpenDownloads = function() {
  audioOpen(RootPath.DOWNLOADS);
};

testcase.galleryOpenDrive = function() {
  galleryOpen(RootPath.DRIVE);
};

testcase.audioOpenDrive = function() {
  audioOpen(RootPath.DRIVE);
};

testcase.zipOpenDownloads = function() {
  zipOpen(RootPath.DOWNLOADS);
};

testcase.zipOpenDrive = function() {
  zipOpen(RootPath.DRIVE);
};
