// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {

function verifyTrack(audioAppId, query, title, artist) {
  var p1 = verifyElenmentTextContent(
              audioAppId,
              query + ' > .data-title',
              title);
  var p2 = verifyElenmentTextContent(
              audioAppId,
              query +' > .data-artist',
              artist);

  return Promise.all(p1, p2).then(function(results) {
    return results.every(function(result) { return result; });
  });
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

      verifyTrack(
          audioAppId,
          'audio-player /deep/ .track[index="0"][active]',
          'Beautiful Song',
          'Unknown artist').then(this.next);
    },
    // Get the source file name.
    function(result) {
      chrome.test.assertTrue(result, 'Displayed data of 1st file is wrong.');

      verifyTrack(
          audioAppId,
          'audio-player /deep/ .track[index="1"]:not([active])',
          'Beautiful Song',
          'Unknown artist').then(this.next);
    },
    // Wait for the changes of the player status.
    function(result) {
      chrome.test.assertTrue(result, 'Displayed data of 2nd file is wrong.');

      // Open another file.
      callRemoteTestUtil(
          'openFile', appId, ['newly added file.ogg'], this.next);
    },
    // Wait for the changes of the player status.
    function(result) {
      chrome.test.assertTrue(result, 'Fail to open the 2nd file');
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

      verifyTrack(
          audioAppId,
          'audio-player /deep/ .track[index="0"][active]',
          'Beautiful Song',
          'Unknown artist').then(this.next);
    },
    // Get the source file name.
    function(result) {
      chrome.test.assertTrue(result, 'Displayed data of 1st file is wrong.');

      verifyTrack(
          audioAppId,
          'audio-player /deep/ .track[index="1"]:not([active])',
          'Beautiful Song',
          'Unknown artist').then(this.next);
    },
    // Wait for the changes of the player status.
    function(result) {
      chrome.test.assertTrue(result, 'Displayed data of 2nd file is wrong.');

      // Close window
      closeWindowAndWait(audioAppId).then(this.next);
    },
    // Wait for the audio player.
    function(result) {
      chrome.test.assertTrue(result, 'Fail to close the window');
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioAutoAdvance(path) {
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

      // Wait for next song.
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
    function(result) {
      chrome.test.assertTrue(result);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioRepeatSingleFile(path) {
  var appId;
  var audioAppId;

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Select the song.
    function(inAppId) {
      appId = inAppId;

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

      callRemoteTestUtil('fakeMouseClick',
                         audioAppId,
                         ['audio-player /deep/ button.repeat input'],
                         this.next);
    },
    function(result) {
      chrome.test.assertTrue(result, 'Failed to click the repeat button');

      var selector = 'audio-player[playing][playcount="1"]';
      waitForElement(audioAppId, selector).then(this.next);
    },
    // Get the source file name.
    function(element) {
      chrome.test.assertEq(
          'filesystem:chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/' +
              'external' + path + '/Beautiful%20Song.ogg',
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
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioNoRepeatSingleFile(path) {
  var appId;
  var audioAppId;

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Select the song.
    function(inAppId) {
      appId = inAppId;

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

      var selector = 'audio-player[playcount="1"]:not([playing])';
      waitForElement(audioAppId, selector).then(this.next);
    },
    // Get the source file name.
    function(element) {
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
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioRepeatMultipleFile(path) {
  var appId;
  var audioAppId;

  var expectedFilesBefore =
      TestEntryInfo.getExpectedRows(path == RootPath.DRIVE ?
          BASIC_DRIVE_ENTRY_SET : BASIC_LOCAL_ENTRY_SET);
  var expectedFilesAfter =
      expectedFilesBefore.concat([ENTRIES.newlyAdded.getExpectedRow()]);

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
      waitForFiles(appId, expectedFilesAfter).then(this.next);
    },
    function(/* no result */) {
      callRemoteTestUtil(
          'openFile', appId, ['newly added file.ogg'], this.next);
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
              'external' + path + '/newly%20added%20file.ogg',
          element.attributes.currenttrackurl);

      callRemoteTestUtil('fakeMouseClick',
                         audioAppId,
                         ['audio-player /deep/ button.repeat input'],
                         this.next);
    },
    function(result) {
      chrome.test.assertTrue(result, 'Failed to click the repeat button');

      // Wait for next song.
      var query = 'audio-player' +
                  '[playing]' +
                  '[currenttrackurl$="Beautiful%20Song.ogg"]';
      waitForElement(audioAppId, query).then(this.next);
    },
    // Get the source file name.
    function(element) {
      chrome.test.assertEq(
          'filesystem:chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/' +
              'external' + path + '/Beautiful%20Song.ogg',
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
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioNoRepeatMultipleFile(path) {
  var appId;
  var audioAppId;

  var expectedFilesBefore =
      TestEntryInfo.getExpectedRows(path == RootPath.DRIVE ?
          BASIC_DRIVE_ENTRY_SET : BASIC_LOCAL_ENTRY_SET);
  var expectedFilesAfter =
      expectedFilesBefore.concat([ENTRIES.newlyAdded.getExpectedRow()]);

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
      waitForFiles(appId, expectedFilesAfter).then(this.next);
    },
    function(/* no result */) {
      callRemoteTestUtil(
          'openFile', appId, ['newly added file.ogg'], this.next);
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
              'external' + path + '/newly%20added%20file.ogg',
          element.attributes.currenttrackurl);

      // Wait for next song.
      var query = 'audio-player:not([playing])';
      waitForElement(audioAppId, query).then(this.next);
    },
    // Get the source file name.
    function(element) {
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

testcase.audioAutoAdvanceDrive = function() {
  audioAutoAdvance(RootPath.DRIVE);
};

testcase.audioRepeatSingleFileDrive = function() {
  audioRepeatSingleFile(RootPath.DRIVE);
};

testcase.audioNoRepeatSingleFileDrive = function() {
  audioNoRepeatSingleFile(RootPath.DRIVE);
};

testcase.audioRepeatMultipleFileDrive = function() {
  audioRepeatMultipleFile(RootPath.DRIVE);
};

testcase.audioNoRepeatMultipleFileDrive = function() {
  audioNoRepeatMultipleFile(RootPath.DRIVE);
};

})();
