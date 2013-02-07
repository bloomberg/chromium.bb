// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generic error callback function for file operations.
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

// Verifies that we are able to create file snapshot.
function verifyFileAccessible(entry, successCallback, errorCallback) {
  chrome.test.assertTrue(typeof(entry.file) != "undefined",
                         "Should have file function.");
  entry.file(function(file) { successCallback(); },
             errorCallback.bind(null, 'Error reading result file.'));
}

// Verifies that we are able to read the directory.
function verifyDirectoryAccessible(entry,
                                   expectedNumChildren,
                                   successCallback,
                                   errorCallback) {
  chrome.test.assertTrue(typeof(entry.createReader) != "undefined");
  var reader = entry.createReader();
  var children = [];

  function onDirectoryRead() {
    chrome.test.assertEq(expectedNumChildren, children.length);
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
