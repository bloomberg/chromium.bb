// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Expected files before tests are performed. Entries for Local tests.
 * @type {Array.<Array.<string>>}
 * @const
 */
var EXPECTED_FILES_BEFORE_LOCAL = [
  ['hello.txt', '51 bytes', 'Plain text', 'Sep 4, 1998 12:34 PM'],
  ['world.ogv', '59 KB', 'OGG video', 'Jul 4, 2012 10:35 AM'],
  ['My Desktop Background.png', '272 bytes', 'PNG image',
      'Jan 18, 2038 1:02 AM'],
  ['Beautiful Song.ogg', '14 KB', 'OGG audio', 'Nov 12, 2086 12:00 PM'],
  ['photos', '--', 'Folder', 'Jan 1, 1980 11:59 PM']
  // ['.warez', '--', 'Folder', 'Oct 26, 1985 1:39 PM']  # should be hidden
].sort();

/**
 * Expected files before tests are performed. Entries for Drive tests.
 * @type {Array.<Array.<string>>}
 * @const
 */
var EXPECTED_FILES_BEFORE_DRIVE = [
  ['hello.txt', '51 bytes', 'Plain text', 'Sep 4, 1998 12:34 PM'],
  ['world.ogv', '59 KB', 'OGG video', 'Jul 4, 2012 10:35 AM'],
  ['My Desktop Background.png', '272 bytes', 'PNG image',
      'Jan 18, 2038 1:02 AM'],
  ['Beautiful Song.ogg', '14 KB', 'OGG audio', 'Nov 12, 2086 12:00 PM'],
  ['photos', '--', 'Folder', 'Jan 1, 1980 11:59 PM'],
  ['Test Document.gdoc','--','Google document','Apr 10, 2013 4:20 PM'],
  ['Test Shared Document.gdoc','--','Google document','Mar 20, 2013 10:40 PM']
].sort();

/**
 * Expected files added during some tests.
 * @type {Array.<Array.<string>>}
 * @const
 */
var EXPECTED_NEWLY_ADDED_FILE = [
  ['newly added file.ogg', '14 KB', 'OGG audio', 'Sep 4, 1998 12:00 AM']
];

/**
 * @param {boolean} isDrive True if the test is for Drive.
 * @return {Array.<Array.<string>>} A sorted list of expected entries at the
 *     initial state.
 */
function getExpectedFilesBefore(isDrive) {
  return isDrive ?
      EXPECTED_FILES_BEFORE_DRIVE :
      EXPECTED_FILES_BEFORE_LOCAL;
}

/**
 * Opens a Files.app's main window and waits until it is initialized.
 *
 * @param {string} path Directory to be opened.
 * @param {function(string, Array.<Array.<string>>)} Callback with the app id
 *     and with the file list.
 */
function setupAndWaitUntilReady(path, callback) {
  callRemoteTestUtil('openMainWindow', null, [path], function(appId) {
    callRemoteTestUtil('waitForFileListChange', appId, [0], function(files) {
      callback(appId, files);
    });
  });
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
 * Expected files shown in "Recent". Directories (e.g. 'photos') are not in this
 * list as they are not expected in "Recent".
 *
 * @type {Array.<Array.<string>>}
 * @const
 */
var EXPECTED_FILES_IN_RECENT = [
  ['hello.txt', '51 bytes', 'Plain text', 'Sep 4, 1998 12:34 PM'],
  ['world.ogv', '59 KB', 'OGG video', 'Jul 4, 2012 10:35 AM'],
  ['My Desktop Background.png', '272 bytes', 'PNG image',
      'Jan 18, 2038 1:02 AM'],
  ['Beautiful Song.ogg', '14 KB', 'OGG audio', 'Nov 12, 2086 12:00 PM'],
  ['Test Document.gdoc','--','Google document','Apr 10, 2013 4:20 PM'],
  ['Test Shared Document.gdoc','--','Google document','Mar 20, 2013 10:40 PM']
].sort();

/**
 * Expected files shown in "Offline", which should have the files
 * "available offline". Google Documents, Google Spreadsheets, and the files
 * cached locally are "available offline".
 * @type {Array.<Array.<string>>}
 * @const
 */
var EXPECTED_FILES_IN_OFFLINE = [
  ['Test Document.gdoc','--','Google document','Apr 10, 2013 4:20 PM'],
  ['Test Shared Document.gdoc','--','Google document','Mar 20, 2013 10:40 PM']
];

/**
 * Expected files shown in "Shared with me", which should be the entries labeled
 * with "shared-with-me".
 * @type {Array.<Array.<string>>}
 * @const
 */
var EXPECTED_FILES_IN_SHARED_WITH_ME = [
  ['Test Shared Document.gdoc','--','Google document','Mar 20, 2013 10:40 PM']
];

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

  var expectedFilesBefore = getExpectedFilesBefore(path == '/drive/root');
  var expectedFilesAfter =
      expectedFilesBefore.concat(EXPECTED_NEWLY_ADDED_FILE).sort();

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(path, this.next);
    },
    // Notify that the list has been verified and a new file can be added
    // in file_manager_browsertest.cc.
    function(inAppId, actualFilesBefore) {
      appId = inAppId;
      chrome.test.assertEq(expectedFilesBefore, actualFilesBefore);
      chrome.test.sendMessage('addEntry', this.next);
    },
    // Confirm that the file has been added externally and wait for it
    // to appear in UI.
    function(reply) {
      chrome.test.assertEq('onEntryAdded', reply);
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
      setupAndWaitUntilReady(path, this.next);
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
      callRemoteTestUtil('selectFile',
                         appId,
                         ['My Desktop Background.png'],
                         this.next);
    },
    // Double click on the label to enter the photo viewer.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil(
          'fakeMouseDoubleClick',
          appId,
          ['#file-list li.table-row[selected] .filename-label span'],
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
      setupAndWaitUntilReady(path, this.next);
    },
    // Select the song.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil(
          'selectFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Double click on the label to enter the audio player.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil(
          'fakeMouseDoubleClick',
          appId,
          ['#file-list li.table-row[selected] .filename-label span'],
          this.next);
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
      setupAndWaitUntilReady(path, this.next);
    },
    function(inAppId) {
      appId = inAppId;
      // Select the song.
      callRemoteTestUtil(
          'selectFile', appId, ['world.ogv'], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      // Double click on the label to enter the video player.
      callRemoteTestUtil(
          'fakeMouseDoubleClick',
          appId,
          ['#file-list li.table-row[selected] .filename-label span'],
          this.next);
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
  // Returns true if |fileList| contains a copy of |filename|.
  var isCopyPresent = function(filename, fileList) {
    var originalEntry;
    for (var i = 0; i < fileList.length; i++) {
      if (getFileName(fileList[i]) == filename)
        originalEntry = fileList[i];
    }
    if (!originalEntry)
      return false;

    var baseName = filename.substring(0, filename.lastIndexOf('.'));
    var extension = filename.substring(filename.lastIndexOf('.'));
    var filenamePattern = new RegExp('^' + baseName + '.+' + extension + '$');
    for (var i = 0; i < fileList.length; i++) {
      // Check size, type and file name pattern to find a copy.
      if (getFileSize(fileList[i]) == getFileSize(originalEntry) &&
          getFileType(fileList[i]) == getFileType(originalEntry) &&
          filenamePattern.exec(getFileName(fileList[i])))
        return true;
    }
    return false;
  };

  var filename = 'world.ogv';
  var appId, fileListBefore;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(path, this.next);
    },
    // Copy the file.
    function(inAppId, inFileListBefore) {
      appId = inAppId;
      fileListBefore = inFileListBefore;
      chrome.test.assertFalse(isCopyPresent(filename, fileListBefore));
      callRemoteTestUtil('copyFile', appId, [filename], this.next);
    },
    // Wait for a file list change.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForFileListChange',
                         appId,
                         [fileListBefore.length],
                         this.next);
    },
    // Verify the result.
    function(fileList) {
      chrome.test.assertTrue(isCopyPresent(filename, fileList));
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
      setupAndWaitUntilReady(path, this.next);
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
      setupAndWaitUntilReady('/drive/root', this.next);
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
          [getExpectedFilesBefore(true /* isDrive */).length],
          this.next);
    },
    // Verify the file list.
    function(actualFilesAfter) {
      chrome.test.assertEq(EXPECTED_FILES_IN_RECENT, actualFilesAfter);
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
      setupAndWaitUntilReady('/drive/root/', this.next)
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
          [getExpectedFilesBefore(true /* isDrive */).length],
          this.next);
    },
    // Verify the file list.
    function(actualFilesAfter) {
      chrome.test.assertEq(EXPECTED_FILES_IN_OFFLINE, actualFilesAfter);
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
      setupAndWaitUntilReady('/drive/root/', this.next);
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
          [getExpectedFilesBefore(true /* isDrive */).length],
          this.next);
    },
    // Verify the file list.
    function(actualFilesAfter) {
      chrome.test.assertEq(EXPECTED_FILES_IN_SHARED_WITH_ME, actualFilesAfter);
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
      setupAndWaitUntilReady('/drive/root', this.next);
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
 * @param {Array.<Array.<string>>} srcContents Expected initial contents in the
 *     source volume.
 * @param {string} dstName Type of destination volume.
 * @param {Array.<Array.<string>>} dstContents Expected initial contents in the
 *     destination volume.
 */
testcase.intermediate.copyBetweenVolumes = function(targetFile,
                                                    srcName,
                                                    srcContents,
                                                    dstName,
                                                    dstContents) {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady('/Downloads', this.next);
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
      setupAndWaitUntilReady('/drive/root/', this.next);
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
 * Tests copy from drive's root to local's downloads.
 */
testcase.transferFromDriveToDownloads = function() {
  testcase.intermediate.copyBetweenVolumes('hello.txt',
                                           'drive',
                                           EXPECTED_FILES_BEFORE_DRIVE,
                                           'downloads',
                                           EXPECTED_FILES_BEFORE_LOCAL);
};

/**
 * Tests copy from local's downloads to drive's root.
 */
testcase.transferFromDownloadsToDrive = function() {
  testcase.intermediate.copyBetweenVolumes('hello.txt',
                                           'downloads',
                                           EXPECTED_FILES_BEFORE_LOCAL,
                                           'drive',
                                           EXPECTED_FILES_BEFORE_DRIVE);
};

/**
 * Tests copy from drive's shared_with_me to local's downloads.
 */
testcase.transferFromSharedToDownloads = function() {
  testcase.intermediate.copyBetweenVolumes('Test Shared Document.gdoc',
                                           'drive_shared_with_me',
                                           EXPECTED_FILES_IN_SHARED_WITH_ME,
                                           'downloads',
                                           EXPECTED_FILES_BEFORE_LOCAL);
};

/**
 * Tests copy from drive's shared_with_me to drive's root.
 */
testcase.transferFromSharedToDrive = function() {
  testcase.intermediate.copyBetweenVolumes('Test Shared Document.gdoc',
                                           'drive_shared_with_me',
                                           EXPECTED_FILES_IN_SHARED_WITH_ME,
                                           'drive',
                                           EXPECTED_FILES_BEFORE_DRIVE);
};

/**
 * Tests copy from drive's recent to local's downloads.
 */
testcase.transferFromRecentToDownloads = function() {
  testcase.intermediate.copyBetweenVolumes('hello.txt',
                                           'drive_recent',
                                           EXPECTED_FILES_IN_RECENT,
                                           'downloads',
                                           EXPECTED_FILES_BEFORE_LOCAL);
};

/**
 * Tests copy from drive's recent to drive's root.
 */
testcase.transferFromRecentToDrive = function() {
  testcase.intermediate.copyBetweenVolumes('hello.txt',
                                           'drive_recent',
                                           EXPECTED_FILES_IN_RECENT,
                                           'drive',
                                           EXPECTED_FILES_BEFORE_DRIVE);
};

/**
 * Tests copy from drive's offline to local's downloads.
 */
testcase.transferFromOfflineToDownloads = function() {
  testcase.intermediate.copyBetweenVolumes('Test Document.gdoc',
                                           'drive_offline',
                                           EXPECTED_FILES_IN_OFFLINE,
                                           'downloads',
                                           EXPECTED_FILES_BEFORE_LOCAL);
};

/**
 * Tests copy from drive's offline to drive's root.
 */
testcase.transferFromOfflineToDrive = function() {
  testcase.intermediate.copyBetweenVolumes('Test Document.gdoc',
                                           'drive_offline',
                                           EXPECTED_FILES_IN_OFFLINE,
                                           'drive',
                                           EXPECTED_FILES_BEFORE_DRIVE);
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

/**
 * Tests hiding the search box.
 */
testcase.hideSearchBox = function() {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady('/Downloads', this.next);
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
                            query: '.search-box-wrapper',
                            styles: {visibility: 'display'}
                          }, {
                            query: '#search-clear-button',
                            styles: {hidden: 'none'}
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
  var EXPECTED_FILES = [
    ['world.ogv', '59 KB', 'OGG video', 'Jul 4, 2012 10:35 AM'],
    ['photos', '--', 'Folder', 'Jan 1, 1980 11:59 PM'],
    ['My Desktop Background.png', '272 bytes', 'PNG image',
     'Jan 18, 2038 1:02 AM'],
    ['hello.txt', '51 bytes', 'Plain text', 'Sep 4, 1998 12:34 PM'],
    ['Beautiful Song.ogg', '14 KB', 'OGG audio', 'Nov 12, 2086 12:00 PM']
  ];
  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady('/Downloads', this.next);
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
                         [EXPECTED_FILES, true /* Check the order */],
                         this.next);
    },
    // Open another window, where the sorted column should be restored.
    function() {
      setupAndWaitUntilReady('/Downloads', this.next);
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
                         [EXPECTED_FILES, true /* Check the order */],
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
      setupAndWaitUntilReady('/Downloads', this.next);
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
      callRemoteTestUtil('fakeEvent',
                         appId,
                         ['#thumbnail-view', 'activate'],
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
      callRemoteTestUtil('openMainWindow', null, ['/Downloads'], this.next);
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
