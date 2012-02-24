// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The ID of this extension.
var fileBrowserExtensionId = 'ddammdhioacbehjngdmkjcjbnfginlla';

var testDirNameSuffix = 'tmp/test_dir_';
var testFileNameSuffix = 'test_file_';

function createRandomName(suffix) {
  return suffix + Math.floor(Math.random() * 10000);
};

function createFileUrl(dirName, fileName) {
  return 'filesystem:chrome-extension://' + fileBrowserExtensionId +
         '/external/' + dirName + '/' + fileName
};

function testFileCreator(filesystem, callback, errorCallback) {
  this.directoryName = createRandomName(testDirNameSuffix);
  this.baseFileName = createRandomName(testFileNameSuffix);
  this.directoryEntry = undefined;
};

// If this.directoryEntry is not already set, creates test directory on the fs.
testFileCreator.prototype.init = function(fs, callback, errorCallback) {
  if (this.directoryEntry)
    callback();

  console.log('Creating directory : ' + this.directoryName);
  fs.root.getDirectory(this.directoryName, {create:true},
                       this.onDirectoryCreated_.bind(this, callback),
                       errorCallback);
};

testFileCreator.prototype.onDirectoryCreated_ = function(callback, dir) {
  console.log('DONE creating directory: ' + dir.fullPath);
  this.directoryEntry = dir;
  callback();
};

testFileCreator.prototype.createFile = function(ext, callback, errorCallback) {
  chrome.test.assertTrue(!!this.directoryEntry,
                         'testFileCreator not initialized');

  var fileName = this.baseFileName + ext;
  console.log('Creating file: ' + fileName);
  this.directoryEntry.getFile(fileName, {create: true},
                              this.createWriter_.bind(this, callback,
                                                            errorCallback),
                              errorCallback);
};

testFileCreator.prototype.createWriter_ = function(callback,
                                                   errorCallback,
                                                   file) {
  file.createWriter(this.writeFile_.bind(this, file, callback, errorCallback),
                    errorCallback);
};

testFileCreator.prototype.writeFile_ = function(file,
                                                callback,
                                                errorCallback,
                                                writer) {
  var randomText = createRandomName('random file text ');

  writer.onerror = errorCallback;
  writer.onwrite = function (progress) {
    console.log('Created file ' + file.fullPath + ' with text: "' +
                randomText + '"');
    callback(file, randomText);
  };

  var bb = new WebKitBlobBuilder();
  bb.append(randomText);
  writer.write(bb.getBlob('text/plain'));
};

testFileCreator.prototype.cleanupAndEndTest = function(successCallback,
                                                       errorCallback) {
  if (!this.directoryEntry)
    return;

  this.directoryEntry.removeRecursively(successCallback, errorCallback);
  this.directoryEntry = undefined;
};

