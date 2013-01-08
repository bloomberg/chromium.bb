// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testStep = [
  function() {
    chrome.syncFileSystem.requestFileSystem('drive', testStep.shift());
  },
  // Create empty file.
  function(fileSystem) {
    fileSystem.root.getFile('Test.txt', {create: true}, testStep.shift(),
        errorHandler);
  },
  // Confirm file is conflicting as this is the mocked value.
  function(fileEntry) {
    chrome.syncFileSystem.getFileSyncStatus(fileEntry, testStep.shift());
  },
  function(fileSyncStatus) {
    chrome.test.assertEq("conflicting", fileSyncStatus);
    chrome.test.succeed();
  }
];

function errorHandler(e) {
  console.log("Failed test with error" + e);
  chrome.test.fail();
}

chrome.test.runTests([
  testStep.shift()
]);
