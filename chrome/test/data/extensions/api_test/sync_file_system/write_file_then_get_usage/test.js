// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var fileEntry;
var fileSystem;
var usageBeforeWrite;
var testData = "12345";

var testStep = [
  function () {
    chrome.syncFileSystem.requestFileSystem(testStep.shift());
  },
  // Create empty file.
  function(fs) {
    fileSystem = fs;
    fileSystem.root.getFile('Test.txt', {create: true}, testStep.shift(),
        errorHandler);
  },
  function(entry) {
    fileEntry = entry;
    testStep.shift()();
  },
  // Record usage before write.
  function() {
    chrome.syncFileSystem.getUsageAndQuota(fileSystem, testStep.shift());
  },
  function(storageInfo) {
    usageBeforeWrite = storageInfo.usageBytes;
    testStep.shift()();
  },
  // Write a known number of bytes.
  function() {
    fileEntry.createWriter(testStep.shift(), errorHandler);
  },
  function (fileWriter) {
    fileWriter.onwriteend = function(e) {
      testStep.shift()();
    };
    fileWriter.onerror = function(e) {
      chrome.test.fail('Write failed: ' + e.toString());
    };
    fileWriter.seek(fileWriter.length);
    var blob = new Blob([testData], {type: "text/plain"});
    fileWriter.write(blob);
  },
  // Check the meta data for updated usage.
  function() {
    fileEntry.getMetadata(testStep.shift(), errorHandler);
  },
  function (metadata) {
    chrome.test.assertEq(testData.length, metadata.size);
    testStep.shift()();
  },
  // Check global usage was updated.
  function() {
    chrome.syncFileSystem.getUsageAndQuota(fileSystem, testStep.shift());
  },
  function(storageInfo) {
    var usageAfterWrite = storageInfo.usageBytes;
    chrome.test.assertEq(testData.length, usageAfterWrite - usageBeforeWrite);
    chrome.test.succeed();
  }
];

function errorHandler() {
  chrome.test.fail();
}

chrome.test.runTests([
  testStep[0]
]);

