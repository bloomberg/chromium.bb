// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function errorCallback(messagePrefix, error) {
  var msg = '';
  if (!error.code) {
    msg = error.message;
  } else {
    switch (error.code) {
      case FileError.QUOTA_EXCEEDED_ERR:
        msg = 'QUOTA_EXCEEDED_ERR';
        break;
      case FileError.NOT_FOUND_ERR:
        msg = 'NOT_FOUND_ERR';
        break;
      case FileError.SECURITY_ERR:
        msg = 'SECURITY_ERR';
        break;
      case FileError.INVALID_MODIFICATION_ERR:
        msg = 'INVALID_MODIFICATION_ERR';
        break;
      case FileError.INVALID_STATE_ERR:
        msg = 'INVALID_STATE_ERR';
        break;
      default:
        msg = 'Unknown Error';
        break;
    }
  }
  chrome.test.fail(messagePrefix + msg);
}

// Verify that we are able to create file snapshot.
function verifyFileAccessible(entry, successCallback, errorCallback) {
  chrome.test.assertTrue(!!entry.file, "Should have file function.");
  entry.file(function(file) { successCallback(); },
             errorCallback.bind(null, 'Error reading result file.'));
}

function verifyDirectoryAccessible(entry,
                                   expectedChildrenNumber,
                                   successCallback,
                                   errorCallback) {
  chrome.test.assertTrue(!!entry.createReader);
  var reader = entry.createReader();
  var children = [];

  function onDirectoryRead() {
    chrome.test.assertEq(expectedChildrenNumber, children.length);
    successCallback();
  }

  function readNext() {
    reader.readEntries(
        function(results) {
          if (results.length == 0) {
            onDirectoryRead();
            return;
          }
          children.push(results);
          readNext();
        },
        errorCallback.bind(null, 'Error reading directory.'));
  }

  readNext();
}

chrome.test.runTests([
  function loadFileSystem() {
  chrome.fileBrowserPrivate.requestLocalFileSystem(
      function (fs) {
        chrome.test.assertTrue(!!fs);
        fs.root.getDirectory('drive', {create: false}, chrome.test.succeed,
            errorCallback.bind(null, 'Unable to get drive root '));
      });
  },
  function driveSearch() {
    chrome.fileBrowserPrivate.searchGData('foo',
        function(entries) {
          chrome.test.assertTrue(!!entries);
          chrome.test.assertEq(2, entries.length);

          chrome.test.assertEq('/drive/Folder',
                               entries[0].fullPath);
          chrome.test.assertEq('/drive/Folder/File.aBc',
                               entries[1].fullPath);

         var directoryVerifier = verifyDirectoryAccessible.bind(null,
             entries[0], 1, chrome.test.succeed, errorCallback);

         verifyFileAccessible(entries[1], directoryVerifier, errorCallback);
        });
  }
]);
