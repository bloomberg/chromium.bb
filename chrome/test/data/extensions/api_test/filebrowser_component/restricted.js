// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Directory that contains all the files used in tests. All files are created
// and cleaned up on C++ side of the test. Initially, all files will have
// content kTestFileContent.
var kTestDirPath = 'mount/test_dir/';
// Test file that will remain unchanged during tests. Used to test read-only
// functionality.
var kTestFilePath = 'mount/test_dir/test_file.foo';
// File that will be used to test delete operation.
var kTestFileToDelete = 'mount/test_dir/test_file_to_delete.foo';
// File that will be used to test write operations.
var kMutableTestFile = 'mount/test_dir/mutable_test_file.foo';
// File that should be created while testing create operation.
// Initialy does not exist.
var kTestFileToCreate = 'mount/test_dir/new_test_file.foo';
// File that will be moved when testing move operation.
var kTestFileToMove = 'mount/test_dir/test_file_to_move.foo';
// Data that will be written when testing write operation.
var kWriteData = '!!!';
// Name of target file in copy operation test.
// The file will be located in the test dir.
var kCopiedFileName = 'copied_file.foo';
// Name of target file in move operation test.
// The file will be located in the test dir.
var kMovedFileName = 'moved_file.foo';
// Initial content of the test files.
var kTestFileContent = "hello, world!";

function TestRunner() {
  this.fileSystem_ = undefined;

  // The entry set in |runGetDirTest|.
  this.testDirEntry = undefined;
  // Read only file under |kTestFilePath|.
  this.testFileEntry = undefined;
  // Last entry set in |runGetFileTest|.
  this.lastFileEntry = undefined;
}

// Sets |testFileEntry| to |lastFileEntry|.
TestRunner.prototype.rememberLastFileEntry = function() {
  chrome.test.assertTrue(!!this.lastFileEntry, 'Cannot set null entry.');
  this.testFileEntry = this.lastFileEntry;
};

// Gets local filesystem used in tests.
TestRunner.prototype.init = function() {
  // Get local FS.
  console.log('Requesting local file system...');
  chrome.fileBrowserPrivate.requestLocalFileSystem(
      this.onFileSystemFetched_.bind(this));
};

TestRunner.prototype.onFileSystemFetched_ = function(fs) {
  chrome.test.assertTrue(!!fs, 'Error getting file system.');

  this.fileSystem_ = fs;
  chrome.test.succeed();
};

TestRunner.prototype.runGetFileTest = function(filePath, create, callback) {
  self = this;
  this.fileSystem_.root.getFile(filePath, {create: create},
      function(entry) {
        self.lastFileEntry = entry;
        callback('getting file', null);
      },
      callback.bind(null, 'getting file'));
};

TestRunner.prototype.runGetDirTest = function(filePath, create, callback) {
  self = this;
  this.fileSystem_.root.getDirectory(filePath, {create: create},
      function(entry) {
        self.testDirEntry = entry;
        callback('getting directory', null);
      },
      callback.bind(null, 'getting directory'));
};

TestRunner.prototype.runReadFileTest = function(entry, expectedText, callback) {
  readFile(entry,
    function(text) {
      chrome.test.assertEq(expectedText, text, 'Unexpected file content');
      callback('reading file', null);
    },
    callback.bind(null, 'reading file'));
};

TestRunner.prototype.runWriteFileTest = function(entry, callback) {
  var self = this;
  entry.createWriter(
    function(writer) {
      writer.onerror = function () {
        callback('writing to file', writer.error);
      };
      writer.onwrite = function(e) {
        callback('writing to file', null);
      };

      writer.write(new Blob([kWriteData], {'type': 'text/plain'}));
    },
    callback.bind(null, 'creating writer'));
};

TestRunner.prototype.runDeleteFileTest = function(entry, callback) {
  entry.remove(
      function() {
        callback('removing file', null);
      },
      callback.bind(null, 'removing file'));
};

TestRunner.prototype.runCopyFileTest = function(sourceEntry,
                                                targetDir,
                                                targetName,
                                                callback) {
  var self = this;
  sourceEntry.copyTo(targetDir, targetName,
      function (entry) {
        callback('copying file', null);
      },
      callback.bind(null, 'copying file'));
};

TestRunner.prototype.runMoveFileTest = function(sourceEntry,
                                                targetDir,
                                                targetName,
                                                callback) {
  var self = this;
  sourceEntry.moveTo(targetDir, targetName,
      function (entry) {
        callback('moving file', null);
      },
      callback.bind(null, 'moving file'));
};

function errorCodeToString(errorCode) {
  switch (errorCode) {
    case FileError.QUOTA_EXCEEDED_ERR:
      return 'QUOTA_EXCEEDED_ERR';
    case FileError.NOT_FOUND_ERR:
      return 'NOT_FOUND_ERR';
    case FileError.SECURITY_ERR:
      return 'SECURITY_ERR';
    case FileError.INVALID_MODIFICATION_ERR:
      return 'INVALID_MODIFICATION_ERR';
    case FileError.INVALID_STATE_ERR:
      return 'INVALID_STATE_ERR';
    default:
      return 'Unknown Error';
  }
}

// Checks operation results and ends the test.
// If the oiperation is supposed to fail, |expectedErrorCode| should be null.
// |error| is error returned by the operation. It is null if the operation
// succeeded.
// |testName| and |operation| parameters are used to make debugging strings
// displayed on test failure clearer and more helpful.
function verifyTestResult(testName, expectedErrorCode, operation, error) {
  if (!error && !expectedErrorCode) {
    chrome.test.succeed();
    return;
  }

  if (expectedErrorCode) {
    chrome.test.assertTrue(!!error,
        testName + ' unexpectedly succeeded. ' +
        'Expected error: ' + errorCodeToString(expectedErrorCode));
  }

  chrome.test.assertEq(expectedErrorCode, error.code,
      testName + ' got unexpected error.'+
      'Expected error: ' + errorCodeToString(expectedErrorCode) + '. ' +
      'Got error' + errorCodeToString(error.code) + ' while ' + operation);

  chrome.test.succeed();
}

chrome.test.runTests([
  function initTests() {
    testRunner = new TestRunner();
    testRunner.init();
  },
  function getFile() {
    testRunner.runGetFileTest(kTestFilePath, false,
        verifyTestResult.bind(null, 'Get Test File', null));
  },
  function saveFile() {
    testRunner.rememberLastFileEntry();
    chrome.test.succeed();
  },
  function getDirectory() {
    testRunner.runGetDirTest(kTestDirPath, false,
        verifyTestResult.bind(null, 'Get Directory', null));
  },
  function readFile() {
    chrome.test.assertTrue(!!testRunner.testFileEntry,
        'Test file entry should have been created by previousd tests.');
    testRunner.runReadFileTest(testRunner.testFileEntry, kTestFileContent,
        verifyTestResult.bind(null, 'Read File', null));
  },
  function copyFile() {
    chrome.test.assertTrue(!!testRunner.testFileEntry,
        'Test file entry should have been created by previousd tests.');
    chrome.test.assertTrue(!!testRunner.testDirEntry,
        'Test dir entry should have been created by previousd tests.');
    testRunner.runCopyFileTest(
        testRunner.testFileEntry,
        testRunner.testDirEntry,
        kCopiedFileName,
        verifyTestResult.bind(null, 'Copy File', FileError.SECURITY_ERR));
  },
  function getFileToMove() {
    testRunner.lastFileEntry = undefined;
    testRunner.runGetFileTest(kTestFileToMove, false,
        verifyTestResult.bind(null, 'Get File To Move', null));
  },
  function moveFile() {
    chrome.test.assertTrue(!!testRunner.lastFileEntry,
        'File entry to move should have been created by previousd tests.');
    chrome.test.assertTrue(!!testRunner.testDirEntry,
        'Test dir entry should have been created by previousd tests.');
    testRunner.runMoveFileTest(
        testRunner.lastFileEntry,
        testRunner.testDirEntry,
        kCopiedFileName,
        verifyTestResult.bind(null, 'Move File', FileError.SECURITY_ERR));
  },
  function getFileToWrite() {
    testRunner.lastFileEntry = undefined;
    testRunner.runGetFileTest(kMutableTestFile, false,
        verifyTestResult.bind(null, 'Get File To Write', null));
  },
  function writeFile() {
    chrome.test.assertTrue(!!testRunner.lastFileEntry,
        'File entry to write should have been created by previousd tests.');
    testRunner.runWriteFileTest(testRunner.lastFileEntry,
        verifyTestResult.bind(null, 'Write File', FileError.SECURITY_ERR));
  },
  function createFile() {
    testRunner.runGetFileTest(kTestFileToCreate, true,
        verifyTestResult.bind(null, 'Create File', FileError.SECURITY_ERR));
  },
  function getFileToDelete() {
    testRunner.lastFileEntry = undefined;
    testRunner.runGetFileTest(kTestFileToDelete, false,
        verifyTestResult.bind(null, 'Get File To Delete', null));
  },
  function deleteFile() {
    chrome.test.assertTrue(!!testRunner.lastFileEntry,
        'File entry to delete should have been created by previousd tests.');
    testRunner.runDeleteFileTest(testRunner.lastFileEntry,
        verifyTestResult.bind(null, 'Delete File', FileError.SECURITY_ERR));
  }
]);
