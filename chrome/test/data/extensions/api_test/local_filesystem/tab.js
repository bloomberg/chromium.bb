// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var fileSystem = null;
var testDirName = null;
var testFileName = null;
var watchfileUrl = null;

function errorCallback(e) {
  var msg = '';
  switch (e.code) {
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
  chrome.test.fail("Got unexpected error: " + msg);
  console.log('Error: ' + msg);
  alert('Error: ' + msg);
}

function successCallbackFinal(entry) {
  chrome.test.succeed();
}

function onGetFileEntry(entry) {
  chrome.test.succeed();
}

function deleteDirectory() {
  console.log("Deleting dir : " + testDirName);
  fileSystem.root.getDirectory(testDirName, {create:false},
      function(directory) {
        // Do clean-up.  (Assume the tab won't be reloaded in testing.)
        directory.removeRecursively(successCallbackFinal, errorCallback);
      }, errorCallback);
}


// Sets up directory watch, kicks of file removal from the directory.
function setupDirectoryWatch(dirEntry) {
  watchfileUrl = dirEntry.toURL();
  chrome.fileBrowserPrivate.addFileWatch(
      watchfileUrl,
      function(success) {
        if (!success) {
          chrome.test.fail("File watch setup failed.");
        } else {
          console.log("Added new file watch: " + watchfileUrl);
          fileSystem.root.getFile(testDirName+'/'+testFileName, {create: false},
              triggerFolderChange, errorCallback);
        }
  });
}

function triggerFolderChange(fileEntry) {
  // Just delete test file, this should trigger onFolderChanged call below.
  fileEntry.remove(function() {}, errorCallback);
}

function onFolderChanged(info) {
  if (info && info.eventType == 'changed' &&
      info.fileUrl == watchfileUrl) {
    // Remove file watch.
    console.log("Removing file watch: " + info.fileUrl);
    chrome.fileBrowserPrivate.removeFileWatch(info.fileUrl,
       function(success) {
         if (success) {
           chrome.test.succeed();
         } else {
           chrome.test.fail("Failed to remove file watch.");
         }
    });
  } else {
    var msg = "ERROR: Unexpected watch event info";
    console.log(msg);
    console.log(info);
    chrome.test.fail(msg);
  }
}

function onLocalFS(fs, error) {
  if (!fs) {
    errorCallback(error);
    return;
  }
  fileSystem = fs;
  console.log("DONE requesting filesystem: " + fileSystem.name);
  // Read directory content.
  console.log("Opening file : " + testDirName+'/'+testFileName);
  fileSystem.root.getDirectory(testDirName, {create: false},
    function(dirEntry) {
      console.log('DONE opening directory: ' + dirEntry.fullPath);
      fileSystem.root.getFile(testDirName+'/'+testFileName, {create:false},
                              onGetFileEntry, errorCallback);
    }, errorCallback);
}

// Initialize tests  - get test dir, file name.
function start() {
  var loc = window.location.href;
  console.log("Opening tab " + loc);
  if (loc.indexOf("#") == -1 ) {
    chrome.test.fail("Missing params");
    return;
  }
  loc = unescape(loc.substr(loc.indexOf("#") + 1));
  if (loc.lastIndexOf("/") == -1 ) {
    chrome.test.fail("Bad params");
    return;
  }
  testDirName = loc.substr(0, loc.lastIndexOf("/"));
  testFileName = loc.substr(loc.lastIndexOf("/") + 1);

  chrome.test.runTests([
    function basic_operations() {
      console.log("Requesting local file system...");
      chrome.fileBrowserPrivate.requestLocalFileSystem(onLocalFS);
    },

    function file_watcher() {
      chrome.fileBrowserPrivate.onFileChanged.addListener(onFolderChanged);
      console.log("Setting up watch of : " + testDirName);
      fileSystem.root.getDirectory(testDirName, {create: false},
          setupDirectoryWatch, errorCallback);
    },

    function cleanup() {
      deleteDirectory();
    }
  ]);
}

start();
