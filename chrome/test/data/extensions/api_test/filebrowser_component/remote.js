// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test file checks if we can read the expected contents
// (kExpectedContents) from the target file (kFileName). What's
// interesting is that we'll read the file via a remote mount point.
// See extension_local_filesystem_apitest.cc for how this is set up.

// These should match the counterparts in
// extension_local_filesystem_apitest.cc.
var kFileName = 'hello.txt';
var kExpectedContents = 'hello, world';

TestRunner.prototype.runTest = function() {
  // Get local FS, create dir with a file in it.
  console.log('Requesting local file system...');
  chrome.fileBrowserPrivate.requestLocalFileSystem(
      this.onFileSystemFetched_.bind(this));
};

TestRunner.prototype.onFileSystemFetched_ = function(fs) {
  if (!fs) {
    this.errorCallback_(chrome.extensions.lastError);
    return;
  }

  this.fileCreator_.init(fs,
                         this.onFileCreatorInit_.bind(this),
                         this.errorCallback_.bind(this));
};

TestRunner.prototype.onFileCreatorInit_ = function() {
  var self = this;
  this.fileCreator_.openFile(
    kFileName,
    function(file, text) {
      readFile(file,
               function(text) {
                 chrome.test.assertEq(kExpectedContents, text);
                 chrome.test.succeed();
               },
               self.errorCallback_.bind(self));
    },
    this.errorCallback_.bind(this));
};

TestRunner.prototype.errorCallback_ = function(error) {
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
  chrome.test.fail(msg);
};

function TestRunner() {
  this.fileCreator_ = new TestFileCreator('tmp',
                                          false /* shouldRandomize */);
}

chrome.test.runTests([function tab() {
  var testRunner = new TestRunner();
  testRunner.runTest();
}]);
