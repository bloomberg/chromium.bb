// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var EXTENSION_ID = 'kidcpjlbjdmcnmccjhjdckhbngnhnepk';
var FILE_CONTENTS = 'hello from test extension.';

function errorCallback(error) {
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
    };
  }
  console.log(msg);
  chrome.test.fail(msg);
}

function getFileSystemUrlForPath(path) {
  return 'filesystem:chrome-extension://' + EXTENSION_ID + '/external/' + path;
}

function writeToFile(entry) {
  entry.createWriter(function(writer) {
    writer.onerror = function(e) {
      errorCallback(writer.error);
    };
    writer.onwrite = chrome.test.succeed;

    var bb = new WebKitBlobBuilder();
    bb.append(FILE_CONTENTS);
    writer.write(bb.getBlob('text/plain'));
  });
}

chrome.test.runTests([function getFile() {
  // The test will try to open and write to a file for which
  // fileBrowserHandler.selectFile has previously been called (on C++ side of
  // the test). It verifies that the permissions given by the method allow the
  // extension to read/write to selected file.
  var entryUrl = getFileSystemUrlForPath('tmp/test_file.txt');
  window.webkitResolveLocalFileSystemURL(entryUrl, writeToFile, errorCallback);
}]);
