// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
This component extension test does the following:

1. Creates a txt and log file on the local file system with some random text.
2. Finds a registered task (file item context menu) for txt file and invokes it
   with url of the test file.
3. Listens for a message from context menu handler and makes sure its payload
   matches the random text from the test file.
*/

// Object used to create files on filesystem. Defined in test_file_util.js.
var fileCreator = new testFileCreator();

var expectedText = undefined;
var cleanupError = 'Got unexpected error while cleaning up test directory.';

function onFileSystemFetched(fs) {
  if (!fs) {
    errorCallback(chrome.extensions.lastError);
    return;
  }

  fileCreator.init(fs, onFileCreatorInit, errorCallback);
};

function onFileCreatorInit() {
  fileCreator.createFile('.log',
      function(file, text) {
        fileCreator.createFile('.tXt',
                               onFileCreated,
                               errorCallback);
      },
      errorCallback);
};

function onFileCreated(file, text) {
  console.log('Get registered tasks now...');
  expectedText = text;
  var fileUrl = file.toURL();
  chrome.fileBrowserPrivate.getFileTasks([fileUrl],
                                         onGetTasks.bind(this, fileUrl));
};

function onGetTasks(fileUrl, tasks) {
  console.log('Tasks: ');
  console.log(tasks);
  if (!tasks || !tasks.length) {
    errorCallback({message: 'No tasks registered'});
    return;
  }

  var expected_tasks = {'TextAction': ['filesystem:*.txt'],
                        'BaseAction': ['filesystem:*', 'filesystem:*.*']};

  console.log('DONE fetching ' + tasks.length + ' tasks');

  if (tasks.length != 2) {
    errorCallback({message: 'Wrong number of tasks found.'});
    return;
  }

  for (var i = 0; i < tasks.length; ++i) {
    var task_name = /^.*[|](\w+)$/.exec(tasks[i].taskId)[1];
    var patterns = tasks[i].patterns;
    var expected_patterns = expected_tasks[task_name];
    if (!expected_patterns) {
      errorCallback({message: 'Wrong task from getFileTasks(): ' + task_name});
      return;
    }
    patterns = patterns.sort();
    expected_patterns = expected_patterns.sort();
    for (var j = 0; j < patterns.length; ++j) {
      if (patterns[j] != expected_patterns[j]) {
        errorCallback({message: 'Wrong patterns set for task ' +
                                task_name + '. ' +
                                'Got: ' + patterns +
                                ' expected: ' + expected_patterns});
        return;
      }
    }
  }

  chrome.fileBrowserPrivate.executeTask(tasks[0].taskId, [fileUrl]);
};

function reportSuccess(entry) {
  chrome.test.succeed();
};

function reportFail(message) {
  chrome.test.fail(message);
};

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

  fileCreator.cleanupAndEndTest(
      reportFail.bind(this, 'Got unexpected error: ' + msg),
      reportFail.bind(this, 'Got unexpected error: ' + msg));
};

// For simple requests:
chrome.extension.onRequestExternal.addListener(
  function(request, sender, sendResponse) {
    if (request.fileContent && request.fileContent == expectedText) {
      sendResponse({success: true});
      fileCreator.cleanupAndEndTest(reportSuccess,
                                    reportFail.bind(this, cleanupError));
    } else {
      sendResponse({success: false});
      console.log('Error message received');
      console.log(request);
      errorCallback(request.error);
    }
  });

chrome.test.runTests([function tab() {
  // Get local FS, create dir with a file in it.
  console.log('Requesting local file system...');
  chrome.fileBrowserPrivate.requestLocalFileSystem(onFileSystemFetched);
}]);
