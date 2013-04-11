// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Expected files before tests are performed.
 * @type {Array.<Array.<string>>}
 * @const
 */
var EXPECTED_FILES_BEFORE = [
  ['hello.txt', '123 bytes', 'Plain text', 'Sep 4, 1998 12:34 PM'],
  ['world.mpeg', '1,000 bytes', 'MPEG video', 'Jul 4, 2012 10:35 AM'],
  ['My Desktop Background.png', '1 KB', 'PNG image', 'Jan 18, 2038 1:02 AM'],
  ['photos', '--', 'Folder', 'Jan 1, 1980 11:59 PM']
  // ['.warez', '--', 'Folder', 'Oct 26, 1985 1:39 PM']  # should be hidden
].sort();

/**
 * Expected files after some tests are performed.
 * @type {Array.<Array.<string>>}
 * @const
 */
var EXPECTED_FILES_AFTER = [
  ['hello.txt', '123 bytes', 'Plain text', 'Sep 4, 1998 12:34 PM'],
  ['world.mpeg', '1,000 bytes', 'MPEG video', 'Jul 4, 2012 10:35 AM'],
  ['My Desktop Background.png', '1 KB', 'PNG image', 'Jan 18, 2038 1:02 AM'],
  ['photos', '--', 'Folder', 'Jan 1, 1980 11:59 PM'],
  // ['.warez', '--', 'Folder', 'Oct 26, 1985 1:39 PM'],  # should be hidden
  ['newly added file.mp3', '2 KB', 'MP3 audio', 'Sep 4, 1998 12:00 AM']
].sort();

/**
 * Waits until the file list is initialized.
 * @param {function(Array.<Array.<string>>)} List of files on the file list.
 */
function waitUntilReady(callback) {
  callRemoteTestUtil('waitForFileListChange', [0], callback);
}

/**
 * Namespace for test cases.
 */
var testcase = {};

/**
 * Tests if the files initially added by the C++ side are displayed, and
 * that a subsequently added file shows up.
 */
testcase.fileDisplay = function() {
  waitUntilReady(function(actualFilesBefore) {
    chrome.test.assertEq(EXPECTED_FILES_BEFORE, actualFilesBefore);
    chrome.test.sendMessage('initial check done', function(reply) {
      chrome.test.assertEq('file added', reply);
      callRemoteTestUtil(
          'waitForFileListChange',
          [EXPECTED_FILES_BEFORE.length],
          chrome.test.callbackPass(function(actualFilesAfter) {
            chrome.test.assertEq(EXPECTED_FILES_AFTER, actualFilesAfter);
          }));
    });
  });
};

/**
 * Tests copying a file to the same directory and waits until the file lists
 * changes.
 */
testcase.keyboardCopy = function() {
  waitUntilReady(function() {
    callRemoteTestUtil('copyFile', ['world.mpeg'], function(result) {
      chrome.test.assertFalse(!result);
      callRemoteTestUtil('waitForFileListChange',
                         [EXPECTED_FILES_BEFORE.length],
                         chrome.test.succeed);
    });
  });
};

/**
 * Tests deleting a file and and waits until the file lists changes.
 */
testcase.keyboardDelete = function() {
  waitUntilReady(function() {
    callRemoteTestUtil('deleteFile', ['world.mpeg'], function(result) {
      chrome.test.assertFalse(!result);
      callRemoteTestUtil('waitAndAcceptDialog', [], function() {
        callRemoteTestUtil('waitForFileListChange',
                           [EXPECTED_FILES_BEFORE.length],
                           chrome.test.succeed);
      });
    });
  });
};
