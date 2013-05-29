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
  var expectedFilesBefore = getExpectedFilesBefore(path == '/drive/root');
  var expectedFilesAfter =
      expectedFilesBefore.concat(EXPECTED_NEWLY_ADDED_FILE).sort();

  setupAndWaitUntilReady(path, function(appId, actualFilesBefore) {
    chrome.test.assertEq(expectedFilesBefore, actualFilesBefore);
    chrome.test.sendMessage('initial check done', function(reply) {
      chrome.test.assertEq('file added', reply);
      callRemoteTestUtil(
          'waitForFileListChange',
          appId,
          [expectedFilesBefore.length], function(actualFilesAfter) {
            chrome.test.assertEq(expectedFilesAfter, actualFilesAfter);
            checkIfNoErrorsOccured(chrome.test.succeed);
          });
    });
  });
};

/**
 * Tests if the gallery shows up for the selected image and that the image
 * gets displayed.
 *
 * @param {string} path Directory path to be tested.
 */
testcase.intermediate.galleryOpen = function(path) {
  var appId;
  var steps = [
    function() {
      setupAndWaitUntilReady(path, steps.shift());
    },
    function(inAppId) {
      appId = inAppId;
      // Resize the window to desired dimensions to avoid flakyness.
      callRemoteTestUtil('resizeWindow', appId, [320, 320], steps.shift());
    },
    function(result) {
      chrome.test.assertTrue(result);
      // Select the image.
      callRemoteTestUtil(
          'selectFile', appId, ['My Desktop Background.png'], steps.shift());
    },
    function(result) {
      chrome.test.assertTrue(result);
      // Click on the label to enter the photo viewer.
      callRemoteTestUtil(
          'fakeMouseClick',
          appId,
          ['#file-list li.table-row[selected] .filename-label span'],
          steps.shift());
    },
    function(result) {
      chrome.test.assertTrue(result);
      // Wait for the image in the gallery's screen image.
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['.gallery .content canvas.image',
                          'iframe.overlay-pane'],
                         steps.shift());
    },
    function(element) {
      // Verify the gallery's screen image.
      chrome.test.assertEq('320', element.attributes.width);
      chrome.test.assertEq('240', element.attributes.height);
      // Get the full-resolution image.
      callRemoteTestUtil('waitForElement',
                         appId,
                         ['.gallery .content canvas.fullres',
                          'iframe.overlay-pane'],
                         steps.shift());
    },
    function(element) {
      // Verify the gallery's screen image.
      chrome.test.assertEq('800', element.attributes.width);
      chrome.test.assertEq('600', element.attributes.height);
      chrome.test.succeed();
    }
  ];
  steps = steps.map(function(f) { return chrome.test.callbackPass(f); });
  steps.shift()();
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
  var steps = [
    function() {
      setupAndWaitUntilReady(path, steps.shift());
    },
    function(inAppId) {
      appId = inAppId;
      // Select the song.
      callRemoteTestUtil(
          'selectFile', appId, ['Beautiful Song.ogg'], steps.shift());
    },
    function(result) {
      chrome.test.assertTrue(result);
      // Click on the label to enter the audio player.
      callRemoteTestUtil(
          'fakeMouseClick',
          appId,
          ['#file-list li.table-row[selected] .filename-label span'],
          steps.shift());
    },
    function(result) {
      chrome.test.assertTrue(result);
      // Wait for the audio player.
      callRemoteTestUtil('waitForWindow',
                         null,
                         ['mediaplayer.html'],
                         steps.shift());
    },
    function(inAppId) {
      audioAppId = inAppId;
      // Wait for the audio tag and verify the source.
      callRemoteTestUtil('waitForElement',
                         audioAppId,
                         ['audio[src]'],
                         steps.shift());
    },
    function(element) {
      chrome.test.assertEq(
          'filesystem:chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/' +
              'external' + path + '/Beautiful%20Song.ogg',
          element.attributes.src);
      // Get the title tag.
      callRemoteTestUtil('waitForElement',
                         audioAppId,
                         ['.data-title'],
                         steps.shift());
    },
    function(element) {
      chrome.test.assertEq('Beautiful Song', element.text);
      // Get the artist tag.
      callRemoteTestUtil('waitForElement',
                         audioAppId,
                         ['.data-artist'],
                         steps.shift());
    },
    function(element) {
      chrome.test.assertEq('Unknown Artist', element.text);
      chrome.test.succeed();
    }
  ];
  steps = steps.map(function(f) { return chrome.test.callbackPass(f); });
  steps.shift()();
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
  var steps = [
    function() {
      setupAndWaitUntilReady(path, steps.shift());
    },
    function(inAppId) {
      appId = inAppId;
      // Select the song.
      callRemoteTestUtil(
          'selectFile', appId, ['world.ogv'], steps.shift());
    },
    function(result) {
      chrome.test.assertTrue(result);
      // Click on the label to enter the video player.
      callRemoteTestUtil(
          'fakeMouseClick',
          appId,
          ['#file-list li.table-row[selected] .filename-label span'],
          steps.shift());
    },
    function(result) {
      chrome.test.assertTrue(result);
      // Wait for the video player.
      callRemoteTestUtil('waitForWindow',
                         null,
                         ['video_player.html'],
                         steps.shift());
    },
    function(inAppId) {
      videoAppId = inAppId;
      // Wait for the video tag and verify the source.
      callRemoteTestUtil('waitForElement',
                         videoAppId,
                         ['video[src]'],
                         steps.shift());
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
                         steps.shift());
    },
    function(element) {
      chrome.test.succeed();
    }
  ];
  steps = steps.map(function(f) { return chrome.test.callbackPass(f); });
  steps.shift()();
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
  }

  var filename = 'world.ogv';
  var appId, fileListBefore;
  var steps = [
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(path, steps.shift());
    },
    // Copy the file.
    function(inAppId, inFileListBefore) {
      appId = inAppId;
      fileListBefore = inFileListBefore;
      chrome.test.assertFalse(isCopyPresent(filename, fileListBefore));
      callRemoteTestUtil('copyFile', appId, [filename], steps.shift());
    },
    // Wait for a file list change.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForFileListChange', appId,
                         [fileListBefore.length], steps.shift());
    },
    // Verify the result.
    function(fileList) {
      chrome.test.assertTrue(isCopyPresent(filename, fileList));
      checkIfNoErrorsOccured(chrome.test.succeed);
    }
  ];
  steps = steps.map(function(f) { return chrome.test.callbackPass(f); });
  steps.shift()();
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
  }

  var filename = 'world.ogv';
  var appId, fileListBefore;
  var steps = [
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(path, steps.shift());
    },
    // Delete the file.
    function(inAppId, inFileListBefore) {
      appId = inAppId;
      fileListBefore = inFileListBefore;
      chrome.test.assertTrue(isFilePresent(filename, fileListBefore));
      callRemoteTestUtil('deleteFile', appId, [filename], steps.shift());
    },
    // Reply to a dialog.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitAndAcceptDialog', appId, [], steps.shift());
    },
    // Wait for a file list change.
    function() {
      callRemoteTestUtil('waitForFileListChange', appId,
                         [fileListBefore.length], steps.shift());
    },
    // Verify the result.
    function(fileList) {
      chrome.test.assertFalse(isFilePresent(filename, fileList));
      checkIfNoErrorsOccured(chrome.test.succeed);
    }
  ];
  steps = steps.map(function(f) { return chrome.test.callbackPass(f); });
  steps.shift()();
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
  var onFileListChange = function(actualFilesAfter) {
    chrome.test.assertEq(EXPECTED_FILES_IN_RECENT, actualFilesAfter);
    checkIfNoErrorsOccured(chrome.test.succeed);
  };

  setupAndWaitUntilReady('/drive/root', function(appId) {
    // Use the icon for a click target.
    callRemoteTestUtil(
        'selectVolume', appId, ['drive_recent'],
        function(result) {
          chrome.test.assertFalse(!result);
          callRemoteTestUtil(
              'waitForFileListChange',
              appId,
              [getExpectedFilesBefore(true /* isDrive */).length],
              onFileListChange);
        });
  });
};

/**
 * Tests opening the "Offline" on the sidebar navigation by clicking the icon,
 * and checks contenets of the file list. Only the entries "available offline"
 * should be shown. "Available offline" entires are hosted documents and the
 * entries cached by DriveCache.
 */
testcase.openSidebarOffline = function() {
  var onFileListChange = function(actualFilesAfter) {
    chrome.test.assertEq(EXPECTED_FILES_IN_OFFLINE, actualFilesAfter);
    checkIfNoErrorsOccured(chrome.test.succeed);
  };

  setupAndWaitUntilReady('/drive/root/', function(appId) {
    // Use the icon for a click target.
    callRemoteTestUtil(
        'selectVolume', appId, ['drive_offline'],
        function(result) {
          chrome.test.assertFalse(!result);
          callRemoteTestUtil(
              'waitForFileListChange',
              appId,
              [getExpectedFilesBefore(true /* isDrive */).length],
              onFileListChange);
        });
  });
};

/**
 * Tests opening the "Shared with me" on the sidebar navigation by clicking the
 * icon, and checks contents of the file list. Only the entries labeled with
 * "shared-with-me" should be shown.
 */
testcase.openSidebarSharedWithMe = function() {
  var onFileListChange = chrome.test.callbackPass(function(actualFilesAfter) {
    chrome.test.assertEq(EXPECTED_FILES_IN_SHARED_WITH_ME, actualFilesAfter);
  });

  setupAndWaitUntilReady('/drive/root/', function(appId) {
    // Use the icon for a click target.
    callRemoteTestUtil(
        'selectVolume', appId, ['drive_shared_with_me'],
        function(result) {
          chrome.test.assertFalse(!result);
          callRemoteTestUtil(
              'waitForFileListChange',
              appId,
              [getExpectedFilesBefore(true /* isDrive */).length],
              onFileListChange);
        });
  });
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

  var onAutocompleteListShown = function(autocompleteList) {
    chrome.test.assertEq(EXPECTED_AUTOCOMPLETE_LIST, autocompleteList);
    checkIfNoErrorsOccured(chrome.test.succeed);
  };

  setupAndWaitUntilReady('/drive/root', function(appId, list) {
    callRemoteTestUtil('performAutocompleteAndWait',
                       appId,
                       ['hello', EXPECTED_AUTOCOMPLETE_LIST.length],
                       onAutocompleteListShown);
  });
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
  var steps = [
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady('/Downloads', steps.shift());
    },
    // Select the source volume.
    function(inAppId) {
      appId = inAppId;
      callRemoteTestUtil('selectVolume', appId, [srcName], steps.shift());
    },
    // Wait for the expected files to appear in the file list.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForFiles', appId, [srcContents], steps.shift());
    },
    // Select the source file.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('selectFile', appId, [targetFile], steps.shift());
    },
    // Copy the file.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('execCommand', appId, ['copy'], steps.shift());
    },
    // Select the destination volume.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('selectVolume', appId, [dstName], steps.shift());
    },
    // Wait for the expected files to appear in the file list.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForFiles', appId, [dstContents], steps.shift());
    },
    // Paste the file.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('execCommand', appId, ['paste'], steps.shift());
    },
    // Wait for the file list to change.
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtil('waitForFileListChange', appId,
                         [dstContents.length], steps.shift());
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
          chrome.test.succeed();
          return;
        }
      }
      chrome.test.fail();
    }
  ];
  steps = steps.map(function(f) { return chrome.test.callbackPass(f); });
  steps.shift()();
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
