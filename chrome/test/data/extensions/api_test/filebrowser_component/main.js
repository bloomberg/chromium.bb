// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
This component extension test does the following:

1. Creates a txt file on the local file system with some random text.
2. Finds a registered task (file item context menu) and invokes it with url
   of the test file.
3. Listens for a message from context menu handler and makes sure its payload
   matches the random text from the test file.
*/

// The ID of this extension.
var fileBrowserExtensionId = "ddammdhioacbehjngdmkjcjbnfginlla";

var fileSystem = null;
var testDirName = "tmp/test_dir_" + Math.floor(Math.random()*10000);
var testFileName = "test_file_" + Math.floor(Math.random()*10000)+".Txt";
var fileUrl = "filesystem:chrome-extension://" + fileBrowserExtensionId +
              "/external/" + testDirName + "/" + testFileName;
var testDirectory = null;
var randomText = "random file text " + Math.floor(Math.random()*10000);

function onFileSystemFetched(fs) {
  if (!fs) {
    errorCallback(chrome.extensions.lastError);
    return;
  }
  fileSystem = fs;
  console.log("DONE requesting local filesystem: " + fileSystem.name);
  console.log("Creating directory : " + testDirName);
  fileSystem.root.getDirectory(testDirName, {create:true},
                               directoryCreateCallback, errorCallback);
}

function directoryCreateCallback(directory) {
  testDirectory = directory;
  console.log("DONE creating directory: " + directory.fullPath);
  directory.getFile(testFileName, {create:true}, fileCreatedCallback,
                    errorCallback);
}

function fileCreatedCallback(fileEntry) {
  console.log("DONE creating file: " + fileEntry.fullPath);
  fileEntry.createWriter(onGetFileWriter);
}

function onGetFileWriter(writer) {
  // Start
  console.log("Got file writer");
  writer.onerror = errorCallback;
  writer.onwrite = onFileWriteCompleted;
  var bb = new WebKitBlobBuilder();
  bb.append(randomText);
  writer.write(bb.getBlob('text/plain'));
}

function onFileWriteCompleted(e) {
  // Start
  console.log("DONE writing file content");
  console.log("Get registered tasks now...");
  chrome.fileBrowserPrivate.getFileTasks([fileUrl], onGetTasks);

}

function onGetTasks(tasks) {
  console.log("Tasks: ");
  console.log(tasks);
  if (!tasks || !tasks.length) {
    chrome.test.fail("No tasks registered");
    return;
  }

  var expected_tasks = {'TextAction': ['filesystem:*.txt'],
                        'BaseAction': ['filesystem:*', 'filesystem:*.*']};

  console.log("DONE fetching " + tasks.length + " tasks");

  if (tasks.length != 2)
    chrome.test.fail('Wrong number of tasks found.');

  for (var i = 0; i < tasks.length; ++i) {
    var task_name = /^.*[|](\w+)$/.exec(tasks[i].taskId)[1];
    var patterns = tasks[i].patterns;
    var expected_patterns = expected_tasks[task_name];
    if (!expected_patterns)
      chrome.test.fail('Wrong task from getFileTasks(): ' + task_name);
    patterns = patterns.sort();
    expected_patterns = expected_patterns.sort();
    for (var j = 0; j < patterns.length; ++j) {
      if (patterns[j] != expected_patterns[j])
        chrome.test.fail('Wrong patterns set for task ' + task_name + '. ' +
                         'Got: ' + patterns +
                         ' expected: ' + expected_patterns);
    }
  }

  chrome.fileBrowserPrivate.executeTask(tasks[0].taskId, [fileUrl]);
}

function errorCallback(e) {
  var msg = '';
  if (!e.code) {
    msg = e.message;
  } else {
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
  }
  chrome.test.fail("Got unexpected error: " + msg);
  console.log('Error: ' + msg);
  alert('Error: ' + msg);
}

function onCleanupFinished(entry) {
  chrome.test.succeed();
}

// For simple requests:
chrome.extension.onRequestExternal.addListener(
  function(request, sender, sendResponse) {
    if (request.fileContent && request.fileContent == randomText) {
      sendResponse({success: true});
      testDirectory.removeRecursively(onCleanupFinished, errorCallback);
    } else {
      sendResponse({success: false});
      console.log('Error message received');
      console.log(request);
      chrome.test.fail("Got error: " + request.error);
    }
  });

chrome.test.runTests([function tab() {
  // Get local FS, create dir with a file in it.
  console.log("Requesting local file system...");
  chrome.fileBrowserPrivate.requestLocalFileSystem(onFileSystemFetched);
}]);
