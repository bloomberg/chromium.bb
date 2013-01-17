// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;

var galleries;
var readTestResults = [];
var writeFileTestResults = [];
var createDirTestResults = [];
var removeFileTestResults = [];
var removeDirTestResults = [];

function checkFinished() {
  if (readTestResults.length != galleries.length)
    return;
  if (writeFileTestResults.length != galleries.length)
    return;
  if (createDirTestResults.length != galleries.length)
    return;
  if (removeFileTestResults.length != galleries.length)
    return;
  if (removeDirTestResults.length != galleries.length)
    return;

  allResults = readTestResults.concat(writeFileTestResults);
  allResults = allResults.concat(createDirTestResults);
  allResults = allResults.concat(removeFileTestResults);
  allResults = allResults.concat(removeDirTestResults);
  var success = true;
  for (var i = 0; i < allResults.length; i++) {
    if (allResults[i]) {
      success = false;
    }
  }
  if (success) {
    chrome.test.succeed();
    return;
  }
  chrome.test.fail(allResults);
}

var mediaFileSystemsDirectoryEntryCallback = function(entries) {
  readTestResults.push("");
  checkFinished();
}

var mediaFileSystemsDirectoryErrorCallback = function(err) {
  readTestResults.push("Couldn't read from directory: " + err);
  checkFinished();
};

var mediaFileSystemsWriteFileCallback = function(fileEntry) {
  fileEntry.createWriter(
      function(fileWriter) {
        fileWriter.onwriteend = function(e) {
          writeFileTestResults.push("");
          checkFinished();
        };
        fileWriter.onerror = function(err) {
          writeFileTestResults.push("Couldn't write file: " + err);
          checkFinished();
        }
        var blob = new Blob(['Lorem Ipsum'], {type: 'text/plain'});
        fileWriter.write(blob);
      },
      function(err) {
        writeFileTestResults.push("createWriter failed: " + err);
        checkFinished();
      });
};

var mediaFileSystemsWriteFileErrorCallback = function(err) {
  writeFileTestResults.push("Couldn't create file: " + err);
  checkFinished();
};

var mediaFileSystemsCreateDirectoryCallback = function(dirEntry) {
  createDirTestResults.push("");
  checkFinished();
};

var mediaFileSystemsCreateDirectoryErrorCallback = function(err) {
  createDirTestResults.push("Couldn't create directory: " + err);
  checkFinished();
};

var mediaFileSystemsRemoveFileCallback = function(fileEntry) {
  fileEntry.remove(
      function() {
        removeFileTestResults.push("");
        checkFinished();
      },
      function(err) {
        removeFileTestResults.push("Couldn't remove file: " + err);
        checkFinished();
      });
};

var mediaFileSystemsRemoveFileErrorCallback = function(err) {
  removeFileTestResults.push("Couldn't get file to remove: " + err);
  checkFinished();
};

var mediaFileSystemsRemoveDirectoryCallback = function(dirEntry) {
  dirEntry.remove(
      function() {
        removeDirTestResults.push("");
        checkFinished();
      },
      function(err) {
        removeDirTestResults.push("Couldn't remove directory: " + err);
        checkFinished();
      });
};

var mediaFileSystemsRemoveDirectoryErrorCallback = function(err) {
  removeDirTestResults.push("Couldn't get directory to remove: " + err);
  checkFinished();
};

function testGalleries(expectedFileSystems) {
  chrome.test.assertEq(expectedFileSystems, galleries.length);
  if (expectedFileSystems == 0) {
    chrome.test.succeed();
    return;
  }

  for (var i = 0; i < galleries.length; i++) {
    var dirReader = galleries[i].root.createReader();
    dirReader.readEntries(mediaFileSystemsDirectoryEntryCallback,
                          mediaFileSystemsDirectoryErrorCallback);
  }

  for (var i = 0; i < galleries.length; i++) {
    galleries[i].root.getFile('test.jpg', {create: true},
                              mediaFileSystemsWriteFileCallback,
                              mediaFileSystemsWriteFileErrorCallback);
  }

  for (var i = 0; i < galleries.length; i++) {
    galleries[i].root.getDirectory('test', {create: true},
                              mediaFileSystemsCreateDirectoryCallback,
                              mediaFileSystemsCreateDirectoryErrorCallback);
  }

  for (var i = 0; i < galleries.length; i++) {
    galleries[i].root.getFile('remove.jpg', {create: true},
                              mediaFileSystemsRemoveFileCallback,
                              mediaFileSystemsRemoveFileErrorCallback);
  }

  for (var i = 0; i < galleries.length; i++) {
    galleries[i].root.getDirectory('test', {},
                              mediaFileSystemsRemoveDirectoryCallback,
                              mediaFileSystemsRemoveDirectoryErrorCallback);
  }
};

var mediaFileSystemsListCallback = function(results) {
  galleries = results;
};

chrome.test.runTests([
  function mediaGalleriesReadAccess() {
    mediaGalleries.getMediaFileSystems(
        chrome.test.callbackPass(mediaFileSystemsListCallback));
  },
]);
