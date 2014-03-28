// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

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

testcase.audioOpenDownloads = function() {
  audioOpen(RootPath.DOWNLOADS);
};

testcase.audioOpenDrive = function() {
  audioOpen(RootPath.DRIVE);
};
