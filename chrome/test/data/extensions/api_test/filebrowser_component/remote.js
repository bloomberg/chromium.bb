// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test file checks if we can read the expected contents
// (kExpectedContents) from the target file (kFileName). We will try to read
// the file directly, and using a filesystem handler
// (chorme/test/data/extensions/api_test/filesystem_handler/). What's
// interesting is that we'll read the file via a remote mount point.
// See external_filesystem_apitest.cc for how this is set up.

// These should match the counterparts in feeds used in
// external_filesystem_apitest.cc.
// The feeds are located in chrome/test/data/chromeos/drive/.
var kDirectoryPath = 'drive/Folder';
var kFileName = 'File.aBc';
var kExpectedContents = 'hello, world\0';
var kNewDirectoryPath = 'drive/FolderNew';

// Gets local filesystem used in tests.
TestRunner.prototype.init = function() {
  // Get local FS.
  console.log('Requesting local file system...');
  chrome.fileBrowserPrivate.requestLocalFileSystem(
      this.onFileSystemFetched_.bind(this));
};

TestRunner.prototype.onFileSystemFetched_ = function(fs) {
  if (!fs) {
    this.errorCallback_(chrome.extensions.lastError,
                        'Error getting file system: ');
    return;
  }

  this.fileSystem_ = fs;
  chrome.test.succeed();
};

TestRunner.prototype.runGetDirTest = function(dirPath, doCreate) {
  self = this;
  chrome.test.assertTrue(!!this.fileSystem_);
  this.fileSystem_.root.getDirectory(dirPath, {create: doCreate},
      function(entry) {
        self.directoryEntry_ = entry;
        chrome.test.succeed();
      },
      self.errorCallback_.bind(self, 'Error creating directory: '));
};

TestRunner.prototype.runReadFileTest = function(fileName, expectedText) {
  var self = this;
  chrome.test.assertTrue(!!this.directoryEntry_);
  this.directoryEntry_.getFile(fileName, {},
      function(entry) {
        self.fileEntry_ = entry;
        readFile(entry,
          function(text) {
            chrome.test.assertEq(expectedText, text);
            chrome.test.succeed();
          },
          self.errorCallback_.bind(self, 'Error reading file: '));
      },
      self.errorCallback_.bind(self, 'Error opening file: '));
};

TestRunner.prototype.runExecuteReadTask = function() {
  chrome.test.assertTrue(!!this.fileEntry_);

  // Add listener to be invoked when filesystem handler extension sends us
  // response.
  this.listener_ = this.onHandlerRequest_.bind(this);
  chrome.extension.onRequestExternal.addListener(this.listener_);

  var self = this;
  var fileURL = this.fileEntry_.toURL();
  chrome.fileBrowserPrivate.getFileTasks([fileURL],
    function(tasks) {
      if (!tasks || !tasks.length) {
        self.errorCallback_({message: 'No tasks registered'},
                            'Error fetching tasks: ');
        return;
      }

      // Execute one of the fetched tasks, and wait for the response from the
      // file handler that will execute it.
      chrome.fileBrowserPrivate.executeTask(tasks[0].taskId, [fileURL]);
    });
};

// Processes the response from file handler for which file task was executed.
TestRunner.prototype.onHandlerRequest_ =
    function(request, sender, sendResponse) {
  // We don't have to listen for a response anymore.
  chrome.extension.onRequestExternal.removeListener(this.listener_);

  this.verifyHandlerRequest(request,
      chrome.test.succeed,
      this.errorCallback_.bind(this, ''));
};

TestRunner.prototype.verifyHandlerRequest =
    function(request, successCallback, errorCallback) {
  if (!request) {
    errorCallback({message: 'Request from handler not defined.'});
    return;
  }

  if (!request.fileContent) {
    var error = request.error || {message: 'Undefined error.'};
    errorCallback(error);
    return;
  }

  if (request.fileContent != kExpectedContents) {
    var error = {message: 'Received content does not match. ' +
                          'Expected ' + originalText + ', ' +
                          'Got "' + request.fileContent + '.'};
    errorCallback(error);
    return;
  }

  successCallback();
};


TestRunner.prototype.errorCallback_ = function(messagePrefix, error) {
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
};

function TestRunner() {
  this.fileSystem_ = undefined;
  this.directoryEntry_ = undefined;
  this.fileEntry_ = undefined;
  this.listener_ = undefined;
}

var testRunner = undefined;

// TODO(tbarzic): Use test runner used in the tests for the local file system
// once needed operations are supported.
chrome.test.runTests([function initTests() {
    testRunner = new TestRunner();
    testRunner.init();
  },
  function readDirectory() {
    // Opens a directory on the gdata mount point.
    // Chrome side of the test will be mocked to think the directory exists.
    testRunner.runGetDirTest(kDirectoryPath, false);
  },
  function readFile() {
    // Opens a file in the directory opened in the previous test..
    // Chrome side of the test will be mocked to think the file exists.
    testRunner.runReadFileTest(kFileName, kExpectedContents);
  },
  function executeReadTask() {
    // Invokes a handler that reads the file opened in the previous test.
    testRunner.runExecuteReadTask();
  },
  function createDir() {
    // Creates new directory.
    testRunner.runGetDirTest(kNewDirectoryPath, true);
  },
]);
