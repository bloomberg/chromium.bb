// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @enum {string}
 * @const
 */
var EntryType = Object.freeze({
  FILE: 'file',
  DIRECTORY: 'directory'
});

/**
 * @enum {string}
 * @const
 */
var SharedOption = Object.freeze({
  NONE: 'none',
  SHARED: 'shared'
});

/**
 * File system entry information for tests.
 *
 * @param {EntryType} type Entry type.
 * @param {string} sourceFileName Source file name that provides file contents.
 * @param {string} targetName Name of entry on the test file system.
 * @param {string} mimeType Mime type.
 * @param {SharedOption} sharedOption Shared option.
 * @param {string} lastModifiedTime Last modified time as a text to be shown in
 *     the last modified column.
 * @param {string} nameText File name to be shown in the name column.
 * @param {string} sizeText Size text to be shown in the size column.
 * @param {string} typeText Type name to be shown in the type column.
 * @constructor
 */
var TestEntryInfo = function(type,
                             sourceFileName,
                             targetPath,
                             mimeType,
                             sharedOption,
                             lastModifiedTime,
                             nameText,
                             sizeText,
                             typeText) {
  this.type = type;
  this.sourceFileName = sourceFileName || '';
  this.targetPath = targetPath;
  this.mimeType = mimeType || '';
  this.sharedOption = sharedOption;
  this.lastModifiedTime = lastModifiedTime;
  this.nameText = nameText;
  this.sizeText = sizeText;
  this.typeText = typeText;
  Object.freeze(this);
};

TestEntryInfo.getExpectedRows = function(entries) {
  return entries.map(function(entry) { return entry.getExpectedRow(); });
};

/**
 * Obtains a expected row contents of the file in the file list.
 */
TestEntryInfo.prototype.getExpectedRow = function() {
  return [this.nameText, this.sizeText, this.typeText, this.lastModifiedTime];
};

/**
 * Filesystem entries used by the test cases.
 * @type {Object.<string, TestEntryInfo>}
 * @const
 */
var ENTRIES = {
  hello: new TestEntryInfo(
      EntryType.FILE, 'text.txt', 'hello.txt',
      'text/plain', SharedOption.NONE, 'Sep 4, 1998 12:34 PM',
      'hello.txt', '51 bytes', 'Plain text'),

  world: new TestEntryInfo(
      EntryType.FILE, 'video.ogv', 'world.ogv',
      'text/plain', SharedOption.NONE, 'Jul 4, 2012 10:35 AM',
      'world.ogv', '59 KB', 'OGG video'),

  unsupported: new TestEntryInfo(
      EntryType.FILE, 'random.bin', 'unsupported.foo',
      'application/x-foo', SharedOption.NONE, 'Jul 4, 2012 10:36 AM',
      'unsupported.foo', '8 KB', 'FOO file'),

  desktop: new TestEntryInfo(
      EntryType.FILE, 'image.png', 'My Desktop Background.png',
      'text/plain', SharedOption.NONE, 'Jan 18, 2038 1:02 AM',
      'My Desktop Background.png', '272 bytes', 'PNG image'),

  beautiful: new TestEntryInfo(
      EntryType.FILE, 'music.ogg', 'Beautiful Song.ogg',
      'text/plain', SharedOption.NONE, 'Nov 12, 2086 12:00 PM',
      'Beautiful Song.ogg', '14 KB', 'OGG audio'),

  photos: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'photos',
      null, SharedOption.NONE, 'Jan 1, 1980 11:59 PM',
      'photos', '--', 'Folder'),

  testDocument: new TestEntryInfo(
      EntryType.FILE, null, 'Test Document',
      'application/vnd.google-apps.document',
      SharedOption.NONE, 'Apr 10, 2013 4:20 PM',
      'Test Document.gdoc', '--', 'Google document'),

  testSharedDocument: new TestEntryInfo(
      EntryType.FILE, null, 'Test Shared Document',
      'application/vnd.google-apps.document',
      SharedOption.SHARED, 'Mar 20, 2013 10:40 PM',
      'Test Shared Document.gdoc', '--', 'Google document'),

  newlyAdded: new TestEntryInfo(
      EntryType.FILE, 'music.ogg', 'newly added file.ogg',
      'audio/ogg', SharedOption.NONE, 'Sep 4, 1998 12:00 AM',
      'newly added file.ogg', '14 KB', 'OGG audio'),

  directoryA: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'A',
      null, SharedOption.NONE, 'Jan 1, 2000 1:00 AM',
      'A', '--', 'Folder'),

  directoryB: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'A/B',
      null, SharedOption.NONE, 'Jan 1, 2000 1:00 AM',
      'B', '--', 'Folder'),

  directoryC: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'A/B/C',
      null, SharedOption.NONE, 'Jan 1, 2000 1:00 AM',
      'C', '--', 'Folder')
};

/**
 * Basic entry set for the local volume.
 * @type {Array.<TestEntryInfo>}
 * @const
 */
var BASIC_LOCAL_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos
];

/**
 * Basic entry set for the drive volume.
 *
 * TODO(hirono): Add a case for an entry cached by FileCache. For testing
 *               Drive, create more entries with Drive specific attributes.
 *
 * @type {Array.<TestEntryInfo>}
 * @const
 */
var BASIC_DRIVE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos,
  ENTRIES.unsupported,
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument
];

var NESTED_ENTRY_SET = [
  ENTRIES.directoryA,
  ENTRIES.directoryB,
  ENTRIES.directoryC
];

/**
 * Expected files shown in "Recent". Directories (e.g. 'photos') are not in this
 * list as they are not expected in "Recent".
 *
 * @type {Array.<TestEntryInfo>}
 * @const
 */
var RECENT_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.unsupported,
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument
];

/**
 * Expected files shown in "Offline", which should have the files
 * "available offline". Google Documents, Google Spreadsheets, and the files
 * cached locally are "available offline".
 *
 * @type {Array.<TestEntryInfo>}
 * @const
 */
var OFFLINE_ENTRY_SET = [
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument
];

/**
 * Expected files shown in "Shared with me", which should be the entries labeled
 * with "shared-with-me".
 *
 * @type {Array.<TestEntryInfo>}
 * @const
 */
var SHARED_WITH_ME_ENTRY_SET = [
  ENTRIES.testSharedDocument
];

/**
 * Opens a Files.app's main window and waits until it is initialized.
 *
 * TODO(hirono): Add parameters to specify the entry set to be prepared.
 *
 * @param {Object} appState App state to be passed with on opening Files.app.
 * @param {function(string, Array.<Array.<string>>)} Callback with the app id
 *     and with the file list.
 */
function setupAndWaitUntilReady(appState, callback) {
  var appId;
  var steps = [
    function() {
      callRemoteTestUtil('openMainWindow', null, [appState], steps.shift());
    },
    function(inAppId) {
      appId = inAppId;
      addEntries(['local'], BASIC_LOCAL_ENTRY_SET, steps.shift());
    },
    function(success) {
      chrome.test.assertTrue(success);
      addEntries(['drive'], BASIC_DRIVE_ENTRY_SET, steps.shift());
    },
    function(success) {
      chrome.test.assertTrue(success);
      callRemoteTestUtil('waitForFileListChange', appId, [0], steps.shift());
    },
    function(fileList) {
      callback(appId, fileList);
    }
  ];
  steps.shift()();
}

/**
 * Verifies if there are no Javascript errors in any of the app windows.
 * @param {function()} Completion callback.
 */
function checkIfNoErrorsOccured(callback) {
  callRemoteTestUtil('getErrorCount', null, [], function(count) {
    chrome.test.assertEq(0, count);
    callback();
  });
}



/**
 * Returns the name of the given file list entry.
 * @param {Array.<string>} file An entry in a file list.
 * @return {string} Name of the file.
 */
function getFileName(fileListEntry) {
  return fileListEntry[0];
}

/**
 * Returns the size of the given file list entry.
 * @param {Array.<string>} An entry in a file list.
 * @return {string} Size of the file.
 */
function getFileSize(fileListEntry) {
  return fileListEntry[1];
}

/**
 * Returns the type of the given file list entry.
 * @param {Array.<string>} An entry in a file list.
 * @return {string} Type of the file.
 */
function getFileType(fileListEntry) {
  return fileListEntry[2];
}

/**
 * Namespace for test cases.
 */
var testcase = {};

/**
 * Namespace for intermediate test cases.
 * */
testcase.intermediate = {};

/**
 * Tests if the files initially added by the C++ side are displayed, and
 * that a subsequently added file shows up.
 *
 * @param {string} path Directory path to be tested.
 */
testcase.intermediate.fileDisplay = function(path) {
  var appId;

  var expectedFilesBefore =
      TestEntryInfo.getExpectedRows(path == '/drive/root' ?
          BASIC_DRIVE_ENTRY_SET : BASIC_LOCAL_ENTRY_SET).sort();

  var expectedFilesAfter =
      expectedFilesBefore.concat([ENTRIES.newlyAdded.getExpectedRow()]).sort();

  StepsRunner.run([
    function() {
      var appState = {defaultPath: path};
      setupAndWaitUntilReady(appState, this.next);
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
      callRemoteTestUtil(
          'waitForFileListChange',
          appId,
          [expectedFilesBefore.length],
          this.next);
    },
    // Confirm the file list.
    function(actualFilesAfter) {
      chrome.test.assertEq(expectedFilesAfter, actualFilesAfter);
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests if the gallery shows up for the selected image and that the image
 * gets displayed.
 *
 * @param {string} path Directory path to be tested.
 */
testcase.intermediate.galleryOpen = function(path) {
  var appId;
  StepsRunner.run([
    function() {
      var appState = {defaultPath: path};
      setupAndWaitUntilReady(appState, this.next);
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
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['.gallery .content canvas.image',
                          'iframe.overlay-pane'],
                         this.next);
    },
    // Verify the gallery's screen image.
    function(element) {
      chrome.test.assertEq('320', element.attributes.width);
      chrome.test.assertEq('240', element.attributes.height);
      // Get the full-resolution image.
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['.gallery .content canvas.fullres',
                          'iframe.overlay-pane'],
                         this.next);
    },
    // Verify the gallery's full resolution image.
    function(element) {
      chrome.test.assertEq('800', element.attributes.width);
      chrome.test.assertEq('600', element.attributes.height);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests if the audio player shows up for the selected image and that the audio
 * is loaded successfully.
 *
 * @param {string} path Directory path to be tested.
 */
testcase.intermediate.audioOpen = function(path) {
  var appId;
  var audioAppId;
  StepsRunner.run([
    function() {
      var appState = {defaultPath: path};
      setupAndWaitUntilReady(appState, this.next);
    },
    // Select the song.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the audio player.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForWindow',
                         null,
                         ['mediaplayer.html'],
                         this.next);
    },
    // Wait for the audio tag and verify the source.
    function(inAppId) {
      audioAppId = inAppId;
      callRemoteTestUtil('waitForElement',
                         audioAppId,
                         ['audio[src]'],
                         this.next);
    },
    // Get the title tag.
    function(element) {
      chrome.test.assertEq(
          'filesystem:chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/' +
              'external' + path + '/Beautiful%20Song.ogg',
          element.attributes.src);
      callRemoteTestUtil('waitForElement',
                         audioAppId,
                         ['.data-title'],
                         this.next);
    },
    // Get the artist tag.
    function(element) {
      chrome.test.assertEq('Beautiful Song', element.text);
      callRemoteTestUtil('waitForElement',
                         audioAppId,
                         ['.data-artist'],
                         this.next);
    },
    // Verify the artist and if there are no javascript errors.
    function(element) {
      chrome.test.assertEq('Unknown Artist', element.text);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests if the video player shows up for the selected movie and that it is
 * loaded successfully.
 *
 * @param {string} path Directory path to be tested.
 */
testcase.intermediate.videoOpen = function(path) {
  var appId;
  var videoAppId;
  StepsRunner.run([
    function() {
      var appState = {defaultPath: path};
      setupAndWaitUntilReady(appState, this.next);
    },
    function(inAppId) {
      appId = inAppId;
      // Select the song.
      callRemoteTestUtil(
          'openFile', appId, ['world.ogv'], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      // Wait for the video player.
      callRemoteTestUtil('waitForWindow',
                         null,
                         ['video_player.html'],
                         this.next);
    },
    function(inAppId) {
      videoAppId = inAppId;
      // Wait for the video tag and verify the source.
      callRemoteTestUtil('waitForElement',
                         videoAppId,
                         ['video[src]'],
                         this.next);
    },
    function(element) {
      chrome.test.assertEq(
          'filesystem:chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/' +
              'external' + path + '/world.ogv',
          element.attributes.src);
      // Wait for the window's inner dimensions. Should be changed to the video
      // size once the metadata is loaded.
      callRemoteTestUtil('waitForWindowGeometry',
                         videoAppId,
                         [320, 192],
                         this.next);
    },
    function(element) {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests copying a file to the same directory and waits until the file lists
 * changes.
 *
 * @param {string} path Directory path to be tested.
 */
testcase.intermediate.keyboardCopy = function(path, callback) {
  var filename = 'world.ogv';
  var expectedFilesBefore =
      TestEntryInfo.getExpectedRows(path == '/drive/root' ?
          BASIC_DRIVE_ENTRY_SET : BASIC_LOCAL_ENTRY_SET).sort();
  var expectedFilesAfter =
      expectedFilesBefore.concat([['world (1).ogv', '59 KB', 'OGG video']]);

  var appId, fileListBefore;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      var appState = {defaultPath: path};
      setupAndWaitUntilReady(appState, this.next);
    },
    // Copy the file.
    function(inAppId, inFileListBefore) {
      appId = inAppId;
      fileListBefore = inFileListBefore;
      chrome.test.assertEq(expectedFilesBefore, inFileListBefore);
      callRemoteTestUtil('copyFile', appId, [filename], this.next);
    },
    // Wait for a file list change.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForFiles',
                         appId,
                         [expectedFilesAfter,
                          {ignoreLastModifiedTime: true}],
                         this.next);
    },
    // Verify the result.
    function(fileList) {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests deleting a file and and waits until the file lists changes.
 * @param {string} path Directory path to be tested.
 */
testcase.intermediate.keyboardDelete = function(path) {
  // Returns true if |fileList| contains |filename|.
  var isFilePresent = function(filename, fileList) {
    for (var i = 0; i < fileList.length; i++) {
      if (getFileName(fileList[i]) == filename)
        return true;
    }
    return false;
  };

  var filename = 'world.ogv';
  var directoryName = 'photos';
  var appId, fileListBefore;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      var appState = {defaultPath: path};
      setupAndWaitUntilReady(appState, this.next);
    },
    // Delete the file.
    function(inAppId, inFileListBefore) {
      appId = inAppId;
      fileListBefore = inFileListBefore;
      chrome.test.assertTrue(isFilePresent(filename, fileListBefore));
      callRemoteTestUtil(
          'deleteFile', appId, [filename], this.next);
    },
    // Reply to a dialog.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil(
          'waitAndAcceptDialog', appId, [], this.next);
    },
    // Wait for a file list change.
    function() {
      callRemoteTestUtil('waitForFileListChange',
                         appId,
                         [fileListBefore.length],
                         this.next);
    },
    // Delete the directory.
    function(fileList) {
      fileListBefore = fileList;
      chrome.test.assertFalse(isFilePresent(filename, fileList));
      chrome.test.assertTrue(isFilePresent(directoryName, fileList));
      callRemoteTestUtil('deleteFile', appId, [directoryName], this.next);
    },
    // Reply to a dialog.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitAndAcceptDialog', appId, [], this.next);
    },
    // Wait for a file list change.
    function() {
      callRemoteTestUtil('waitForFileListChange', appId,
                         [fileListBefore.length], this.next);
    },
    // Verify the result.
    function(fileList) {
      chrome.test.assertFalse(isFilePresent(directoryName, fileList));
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

testcase.fileDisplayDownloads = function() {
  testcase.intermediate.fileDisplay('/Downloads');
};

testcase.galleryOpenDownloads = function() {
  testcase.intermediate.galleryOpen('/Downloads');
};

testcase.audioOpenDownloads = function() {
  testcase.intermediate.audioOpen('/Downloads');
};

testcase.videoOpenDownloads = function() {
  testcase.intermediate.videoOpen('/Downloads');
};

testcase.keyboardCopyDownloads = function() {
  testcase.intermediate.keyboardCopy('/Downloads');
};

testcase.keyboardDeleteDownloads = function() {
  testcase.intermediate.keyboardDelete('/Downloads');
};

testcase.fileDisplayDrive = function() {
  testcase.intermediate.fileDisplay('/drive/root');
};

testcase.galleryOpenDrive = function() {
  testcase.intermediate.galleryOpen('/drive/root');
};

testcase.audioOpenDrive = function() {
  testcase.intermediate.audioOpen('/drive/root');
};

testcase.videoOpenDrive = function() {
  testcase.intermediate.videoOpen('/drive/root');
};

testcase.keyboardCopyDrive = function() {
  testcase.intermediate.keyboardCopy('/drive/root');
};

testcase.keyboardDeleteDrive = function() {
  testcase.intermediate.keyboardDelete('/drive/root');
};

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
      var appState = {defaultPath: '/drive/root'};
      setupAndWaitUntilReady(appState, this.next);
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
      var appState = {defaultPath: '/drive/root/'};
      setupAndWaitUntilReady(appState, this.next);
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
      var appState = {defaultPath: '/drive/root/'};
      setupAndWaitUntilReady(appState, this.next);
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
      var appState = {defaultPath: '/drive/root'};
      setupAndWaitUntilReady(appState, this.next);
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
testcase.intermediate.copyBetweenVolumes = function(targetFile,
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
      var appState = {defaultPath: '/Downloads'};
      setupAndWaitUntilReady(appState, this.next);
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
};

/**
 * Test sharing dialog for a file or directory on Drive
 * @param {string} path Path for a file or a directory to be shared.
 */
testcase.intermediate.share = function(path) {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      var appState = {defaultPath: '/drive/root/'};
      setupAndWaitUntilReady(appState, this.next);
    },
    // Select the source file.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil(
          'selectFile', appId, [path], this.next);
    },
    // Wait for the share button.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['#share-button:not([disabled])'],
                         this.next);
    },
    // Invoke the share dialog.
    function(result) {
      callRemoteTestUtil('fakeMouseClick',
                         appId,
                         ['#share-button'],
                         this.next);
    },
    // Wait until the share dialog's contents are shown.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['.share-dialog-webview-wrapper.loaded'],
                         this.next);
    },
    function(result) {
      callRemoteTestUtil('waitForStyles',
                         appId,
                         [{
                            query: '.share-dialog-webview-wrapper.loaded',
                            styles: {
                              width: '350px',
                              height: '250px'
                            }
                          }],
                         this.next);
    },
    // Wait until the share dialog's contents are shown.
    function(result) {
      callRemoteTestUtil('executeScriptInWebView',
                         appId,
                         ['.share-dialog-webview-wrapper.loaded webview',
                          'document.querySelector("button").click()'],
                         this.next);
    },
    // Wait until the share dialog's contents are hidden.
    function(result) {
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['.share-dialog-webview-wrapper.loaded',
                          null,   // iframeQuery
                          true],  // inverse
                         this.next);
    },
    // Check for Javascript errros.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Test utility for traverse tests.
 */
testcase.intermediate.traverseDirectories = function(root) {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      var appState = {defaultPath: root};
      callRemoteTestUtil('openMainWindow', null, [appState], this.next);
    },
    // Check the initial view.
    function(inAppId) {
      appId = inAppId;
      addEntries(['local', 'drive'], NESTED_ENTRY_SET, this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForFiles',
                         appId,
                         [[ENTRIES.directoryA.getExpectedRow()]],
                         this.next);
    },
    // Open the directory
    function() {
      callRemoteTestUtil('openFile', appId, ['A'], this.next);
    },
    // Check the contents of current directory.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForFiles',
                         appId,
                         [[ENTRIES.directoryB.getExpectedRow()]],
                         this.next);
    },
    // Open the directory
    function() {
      callRemoteTestUtil('openFile', appId, ['B'], this.next);
    },
    // Check the contents of current directory.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForFiles',
                         appId,
                         [[ENTRIES.directoryC.getExpectedRow()]],
                         this.next);
    },
    // Check the error.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests copy from drive's root to local's downloads.
 */
testcase.transferFromDriveToDownloads = function() {
  testcase.intermediate.copyBetweenVolumes('hello.txt',
                                           'drive',
                                           BASIC_DRIVE_ENTRY_SET,
                                           'downloads',
                                           BASIC_LOCAL_ENTRY_SET);
};

/**
 * Tests copy from local's downloads to drive's root.
 */
testcase.transferFromDownloadsToDrive = function() {
  testcase.intermediate.copyBetweenVolumes('hello.txt',
                                           'downloads',
                                           BASIC_LOCAL_ENTRY_SET,
                                           'drive',
                                           BASIC_DRIVE_ENTRY_SET);
};

/**
 * Tests copy from drive's shared_with_me to local's downloads.
 */
testcase.transferFromSharedToDownloads = function() {
  testcase.intermediate.copyBetweenVolumes('Test Shared Document.gdoc',
                                           'drive_shared_with_me',
                                           SHARED_WITH_ME_ENTRY_SET,
                                           'downloads',
                                           BASIC_LOCAL_ENTRY_SET);
};

/**
 * Tests copy from drive's shared_with_me to drive's root.
 */
testcase.transferFromSharedToDrive = function() {
  testcase.intermediate.copyBetweenVolumes('Test Shared Document.gdoc',
                                           'drive_shared_with_me',
                                           SHARED_WITH_ME_ENTRY_SET,
                                           'drive',
                                           BASIC_DRIVE_ENTRY_SET);
};

/**
 * Tests copy from drive's recent to local's downloads.
 */
testcase.transferFromRecentToDownloads = function() {
  testcase.intermediate.copyBetweenVolumes('hello.txt',
                                           'drive_recent',
                                           RECENT_ENTRY_SET,
                                           'downloads',
                                           BASIC_LOCAL_ENTRY_SET);
};

/**
 * Tests copy from drive's recent to drive's root.
 */
testcase.transferFromRecentToDrive = function() {
  testcase.intermediate.copyBetweenVolumes('hello.txt',
                                           'drive_recent',
                                           RECENT_ENTRY_SET,
                                           'drive',
                                           BASIC_DRIVE_ENTRY_SET);
};

/**
 * Tests copy from drive's offline to local's downloads.
 */
testcase.transferFromOfflineToDownloads = function() {
  testcase.intermediate.copyBetweenVolumes('Test Document.gdoc',
                                           'drive_offline',
                                           OFFLINE_ENTRY_SET,
                                           'downloads',
                                           BASIC_LOCAL_ENTRY_SET);
};

/**
 * Tests copy from drive's offline to drive's root.
 */
testcase.transferFromOfflineToDrive = function() {
  testcase.intermediate.copyBetweenVolumes('Test Document.gdoc',
                                           'drive_offline',
                                           OFFLINE_ENTRY_SET,
                                           'drive',
                                           BASIC_DRIVE_ENTRY_SET);
};

/**
 * Tests sharing a file on Drive
 */
testcase.shareFile = function() {
  testcase.intermediate.share('world.ogv');
};

/**
 * Tests sharing a directory on Drive
 */
testcase.shareDirectory = function() {
  testcase.intermediate.share('photos');
};

testcase.executeDefaultTaskOnDrive = function(root) {
  testcase.intermediate.executeDefaultTask(true);
};

testcase.executeDefaultTaskOnDownloads = function(root) {
  testcase.intermediate.executeDefaultTask(false);
};

/**
 * Tests executing the default task when there is only one task.
 */
testcase.intermediate.executeDefaultTask = function(drive) {
  var root = drive ? '/drive/root' : '/Downloads';
  var taskId = drive ? 'dummytaskid|drive|open-with' : 'dummytaskid|open-with'
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      var appState = {
        defaultPath: root
      };
      setupAndWaitUntilReady(appState, this.next);
    },
    // Override tasks list with a dummy task.
    function(inAppId, inFileListBefore) {
      appId = inAppId;

      callRemoteTestUtil(
          'overrideTasks',
          appId,
          [[
            {
              driveApp: false,
              iconUrl: 'chrome://theme/IDR_DEFAULT_FAVICON',  // Dummy icon
              isDefault: true,
              taskId: taskId,
              title: 'The dummy task for test'
            }
          ]],
          this.next);
    },
    // Select file.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil(
          'selectFile', appId, ['hello.txt'], this.next);
    },
    // Double-click the file.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil(
          'fakeMouseDoubleClick',
          appId,
          ['#file-list li.table-row[selected] .filename-label span'],
          this.next);
    },
    // Wait until the task is executed.
    function(result) {
      chrome.test.assertTrue(!!result);
      callRemoteTestUtil(
          'waitUntilTaskExecutes',
          appId,
          [taskId],
          this.next);
    },
    // Check the error.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests sharing a file on Drive
 */
testcase.suggestAppDialog = function() {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      chrome.test.sendMessage(
          JSON.stringify({name: 'getCwsWidgetContainerMockUrl'}),
          this.next);
    },
    // Override the container URL with the mock.
    function(json) {
      var data = JSON.parse(json);

      var appState = {
        defaultPath: '/drive/root',
        suggestAppsDialogState: {
          overrideCwsContainerUrlForTest: data.url,
          overrideCwsContainerOriginForTest: data.origin
        }
      };
      setupAndWaitUntilReady(appState, this.next);
    },
    function(inAppId, inFileListBefore) {
      appId = inAppId;

      callRemoteTestUtil(
          'selectFile', appId, ['unsupported.foo'], this.next);
    },
    // Double-click the file.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil(
          'fakeMouseDoubleClick',
          appId,
          ['#file-list li.table-row[selected] .filename-label span'],
          this.next);
    },
    // Wait for the widget is loaded.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil(
          'waitForElement',
          appId,
          ['#suggest-app-dialog webview[src]'],
          this.next);
    },
    // Wait for the widget is initialized.
    function(result) {
      chrome.test.assertTrue(!!result);
      callRemoteTestUtil(
          'waitForElement',
          appId,
          ['#suggest-app-dialog:not(.show-spinner)'],
          this.next);
    },
    // Override task APIs for test.
    function(result) {
      chrome.test.assertTrue(!!result);
      callRemoteTestUtil(
          'overrideTasks',
          appId,
          [[
            {
              driveApp: false,
              iconUrl: 'chrome://theme/IDR_DEFAULT_FAVICON',  // Dummy icon
              isDefault: true,
              taskId: 'dummytaskid|drive|open-with',
              title: 'The dummy task for test'
            }
          ]],
          this.next);
    },
    // Override installWebstoreItem API for test.
    function(result) {
      chrome.test.assertTrue(!!result);
      callRemoteTestUtil(
          'overrideInstallWebstoreItemApi',
          appId,
          [
            'DUMMY_ITEM_ID_FOR_TEST',  // Same ID in cws_container_mock/main.js.
            null  // Success
          ],
          this.next);
    },
    // Initiate an installation from the widget.
    function(result) {
      chrome.test.assertTrue(!!result);
      callRemoteTestUtil(
          'executeScriptInWebView',
          appId,
          ['#suggest-app-dialog webview',
           'document.querySelector("button").click()'],
          this.next);
    },
    // Wait until the installation is finished and the dialog is closed.
    function(result) {
      chrome.test.assertTrue(!!result);
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['#suggest-app-dialog',
                          null,   // iframeQuery
                          true],  // inverse
                         this.next);
    },
    // Wait until the task is executed.
    function(result) {
      chrome.test.assertTrue(!!result);
      callRemoteTestUtil(
          'waitUntilTaskExecutes',
          appId,
          ['dummytaskid|drive|open-with'],
          this.next);
    },
    // Check error
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests hiding the search box.
 */
testcase.hideSearchBox = function() {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      var appState = {defaultPath: '/Downloads'};
      setupAndWaitUntilReady(appState, this.next);
    },
    // Resize the window.
    function(inAppId, inFileListBefore) {
      appId = inAppId;
      callRemoteTestUtil('resizeWindow', appId, [100, 100], this.next);
    },
    // Wait for the style change.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForStyles',
                         appId,
                         [{
                            query: '#search-box',
                            styles: {visibility: 'hidden'}
                          }],
                         this.next);
    },
    // Check the styles
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests restoring the sorting order.
 */
testcase.restoreSortColumn = function() {
  var appId;
  var EXPECTED_FILES = TestEntryInfo.getExpectedRows([
    ENTRIES.world,
    ENTRIES.photos,
    ENTRIES.desktop,
    ENTRIES.hello,
    ENTRIES.beautiful
  ]);
  StepsRunner.run([
    // Set up File Manager.
    function() {
      var appState = {defaultPath: '/Downloads'};
      setupAndWaitUntilReady(appState, this.next);
    },
    // Sort by name.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil('fakeMouseClick',
                         appId,
                         ['.table-header-cell:nth-of-type(1)'],
                         this.next);
    },
    // Check the sorted style of the header.
    function() {
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['.table-header-sort-image-asc'],
                         this.next);
    },
    // Sort by name.
    function() {
      callRemoteTestUtil('fakeMouseClick',
                         appId,
                         ['.table-header-cell:nth-of-type(1)'],
                         this.next);
    },
    // Check the sorted style of the header.
    function() {
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['.table-header-sort-image-desc'],
                         this.next);
    },
    // Check the sorted files.
    function() {
      callRemoteTestUtil('waitForFiles',
                         appId,
                         [EXPECTED_FILES, {orderCheck: true}],
                         this.next);
    },
    // Open another window, where the sorted column should be restored.
    function() {
      var appState = {defaultPath: '/Downloads'};
      setupAndWaitUntilReady(appState, this.next);
    },
    // Check the sorted style of the header.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['.table-header-sort-image-desc'],
                         this.next);
    },
    // Check the sorted files.
    function() {
      callRemoteTestUtil('waitForFiles',
                         appId,
                         [EXPECTED_FILES, {orderCheck: true}],
                         this.next);
    },
    // Check the error.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests restoring the current view (the file list or the thumbnail grid).
 */
testcase.restoreCurrentView = function() {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      var appState = {defaultPath: '/Downloads'};
      setupAndWaitUntilReady(appState, this.next);
    },
    // Check the initial view.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['.thumbnail-grid[hidden]'],
                         this.next);
    },
    // Change the current view.
    function() {
      callRemoteTestUtil('fakeMouseClick',
                         appId,
                         ['#thumbnail-view'],
                         this.next);
    },
    // Check the new current view.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['.detail-table[hidden]'],
                         this.next);
    },
    // Open another window, where the current view is restored.
    function() {
      var appState = {defaultPath: '/Downloads'};
      callRemoteTestUtil('openMainWindow', null, [appState], this.next);
    },
    // Check the current view.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['.detail-table[hidden]'],
                         this.next);
    },
    // Check the error.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests keyboard operations of the navigation list.
 */
testcase.traverseNavigationList = function() {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      var appState = {defaultPath: '/drive/root'};
      setupAndWaitUntilReady(appState, this.next);
    },
    // Wait for the navigation list.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil(
          'waitForElement',
          appId,
          ['#navigation-list > .root-item > ' +
               '.volume-icon[volume-type-icon="drive"]'],
          this.next);
    },
    // Ensure that the 'Gogole Drive' is selected.
    function() {
      callRemoteTestUtil('checkSelectedVolume',
                         appId,
                         ['Google Drive', '/drive/root'],
                         this.next);
    },
    // Ensure that the current directory is changed to 'Gogole Drive'.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForFiles',
                         appId,
                         [TestEntryInfo.getExpectedRows(BASIC_DRIVE_ENTRY_SET),
                          {ignoreLastModifiedTime: true}],
                         this.next);
    },
    // Press the UP key.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('fakeKeyDown',
                         appId,
                         ['#navigation-list', 'Up', true],
                         this.next);
    },
    // Ensure that the 'Gogole Drive' is still selected since it is the first
    // item.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('checkSelectedVolume',
                         appId,
                         ['Google Drive', '/drive/root'],
                         this.next);
    },
    // Press the DOWN key.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('fakeKeyDown',
                         appId,
                         ['#navigation-list', 'Down', true],
                         this.next);
    },
    // Ensure that the 'Download' is selected.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('checkSelectedVolume',
                         appId,
                         ['Downloads', '/Downloads'],
                         this.next);
    },
    // Ensure that the current directory is changed to 'Downloads'.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForFiles',
                         appId,
                         [TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET),
                          {ignoreLastModifiedTime: true}],
                         this.next);
    },
    // Press the DOWN key again.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('fakeKeyDown',
                         appId,
                         ['#navigation-list', 'Down', true],
                         this.next);
    },
    // Ensure that the 'Downloads' is still selected since it is the last item.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('checkSelectedVolume',
                         appId,
                         ['Downloads', '/Downloads'],
                         this.next);
    },
    // Check for errors.
    function(result) {
      chrome.test.assertTrue(result);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests restoring geometry of the Files app.
 */
testcase.restoreGeometry = function() {
  var appId;
  var appId2;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      var appState = {defaultPath: '/Downloads'};
      setupAndWaitUntilReady(appState, this.next);
    },
    // Resize the window to minimal dimensions.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil(
          'resizeWindow', appId, [640, 480], this.next);
    },
    // Check the current window's size.
    function(inAppId) {
      callRemoteTestUtil('waitForWindowGeometry',
                         appId,
                         [640, 480],
                         this.next);
    },
    // Enlarge the window by 10 pixels.
    function(result) {
      callRemoteTestUtil(
          'resizeWindow', appId, [650, 490], this.next);
    },
    // Check the current window's size.
    function() {
      callRemoteTestUtil('waitForWindowGeometry',
                         appId,
                         [650, 490],
                         this.next);
    },
    // Open another window, where the current view is restored.
    function() {
      var appState = {defaultPath: '/Downloads'};
      setupAndWaitUntilReady(appState, this.next);
    },
    // Check the next window's size.
    function(inAppId) {
      appId2 = inAppId;
      callRemoteTestUtil('waitForWindowGeometry',
                         appId2,
                         [650, 490],
                         this.next);
    },
    // Check for errors.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests to traverse local directories.
 */
testcase.traverseDownloads =
    testcase.intermediate.traverseDirectories.bind(null, '/Downloads');

/**
 * Tests to traverse drive directories.
 */
testcase.traverseDrive =
    testcase.intermediate.traverseDirectories.bind(null, '/drive/root');

/**
 * Tests the focus behavior of the search box.
 */
testcase.searchBoxFocus = function() {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      var appState = {defaultPath: '/drive/root'};
      setupAndWaitUntilReady(appState, this.next);
    },
    // Check that the file list has the focus on launch.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil(
          'waitForElement', appId, ['#file-list:focus'], this.next);
    },
    // Press the Ctrl-F key.
    function(element) {
      callRemoteTestUtil('fakeKeyDown',
                         appId,
                         ['body', 'U+0046', true],
                         this.next);
    },
    // Check that the search box has the focus.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil(
          'waitForElement', appId, ['#search-box input:focus'], this.next);
    },
    // Press the Tab key.
    function(element) {
      callRemoteTestUtil('fakeKeyDown',
                         appId,
                         ['body', 'U+0009', false],
                         this.next);
    },
    // Check that the file list has the focus.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil(
          'waitForElement', appId, ['#file-list:focus'], this.next);
    },
    // Check for errors.
    function(element) {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests if a thumbnail for the selected item shows up in the preview panel.
 * This thumbnail is fetched via the image loader.
 */
testcase.thumbnailsDownloads = function() {
  var appId;
  StepsRunner.run([
    function() {
      var appState = {defaultPath: '/Downloads'};
      setupAndWaitUntilReady(appState, this.next);
    },
    // Select the image.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil('selectFile',
                         appId,
                         ['My Desktop Background.png'],
                         this.next);
    },
    // Wait until the thumbnail shows up.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['.preview-thumbnails .img-container img'],
                         this.next);
    },
    // Verify the thumbnail.
    function(element) {
      chrome.test.assertTrue(element.attributes.src.indexOf(
          'data:image/jpeg') === 0);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
