// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var fileSystem;

var testStep = [
  // First get the file system.
  function () {
    chrome.syncFileSystem.requestFileSystem(testStep.shift());
  },
  // Add a couple of test files.
  function(fs) {
    fileSystem = fs;
    fileSystem.root.getFile('Test1.txt', {create: true}, testStep.shift(),
        errorHandler);
  },
  function(fileEntry) {
    fileSystem.root.getFile('Test2.txt', {create: true}, testStep.shift(),
        errorHandler);
  },
  // Check the directory for contents.
  function(dirEntry) {
    var dirReader = fileSystem.root.createReader();
    dirReader.readEntries(testStep.shift(), errorHandler);
  },
  function(entries) {
    chrome.test.assertEq(2, entries.length);
    var fileList = [];
    fileList.push(entries[0].fullPath);
    fileList.push(entries[1].fullPath);
    fileList.sort();

    chrome.test.assertEq("/Test1.txt", fileList[0]);
    chrome.test.assertEq("/Test2.txt", fileList[1]);
    testStep.shift()();
  },
  // Then try to delete the file system.
  function() {
    chrome.syncFileSystem.deleteFileSystem(fileSystem, testStep.shift());
  },
  function(result) {
    chrome.test.assertTrue(result);
    testStep.shift()();
  },
  // Assert that the file system no longer exists.
  function(fileEntry) {
    fileSystem.root.getDirectory('/', {}, testFail, testStep.shift());
  },
  function(e) {
    chrome.test.assertEq(FileError.NOT_FOUND_ERR, e.code);
    chrome.test.succeed();
  }
];

function testFail() {
  chrome.test.fail("Failure callback should have been called instead as " +
      "file system should no longer exist.");
}

function errorHandler(e) {
  chrome.test.fail("e=" + e.code);
}

chrome.test.runTests([
  testStep[0]
]);
