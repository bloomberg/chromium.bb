// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var fileSystem = null;
var testDirName = "tmp/test_dir_" + Math.floor(Math.random()*10000);
var testFileName = "test_file_" + Math.floor(Math.random()*10000);

// Get local FS, create dir with a file in it.
console.log("Requesting local file system...");
chrome.fileBrowserPrivate.requestLocalFileSystem(getFileSystem);

function getFileSystem(fs, error) {
  if (!fs) {
    errorCallback(error);
    return;
  }
  fileSystem = fs;
  console.log("DONE requesting local filesystem: " + fileSystem.name);
  console.log("Creating directory : " + testDirName);
  fileSystem.root.getDirectory(testDirName, {create:true},
                               directoryCallback, errorCallback);
}

function directoryCallback(directory) {
  console.log("DONE creating directory: " + directory.fullPath);
  directory.getFile(testFileName, {create:true}, fileCallback, errorCallback);
}

function fileCallback(file) {
  console.log("DONE creating file: " + file.fullPath);

  // See if we can access this filesystem and its elements in the tab.
  console.log("Opening tab...");
  chrome.tabs.create({
    url: "tab.html#" + escape(testDirName + "/" + testFileName)
  });
}

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
