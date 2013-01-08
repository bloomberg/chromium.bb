// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
This component extension test does the following:

1. Creates an abc and log file on the local file system with some random text.
2. Finds a registered task (file item context menu) for abc file and invokes it
   with url of the test file.
3. Listens for a message from context menu handler and makes sure its payload
   matches the random text from the test file.
*/

var cleanupError = 'Got unexpected error while cleaning up test directory.';
var kFileManagerExtensionId = 'hhaomjibdihmijegdhdafkllkbggdgoj';

// Class specified by the client running the TestRunner.
// |expectedTasks| should contain list of actions defined for abc files defined
//     by filesystem_handler part of the test.
// |fileVerifierFunction| method that will verify test results received from the
//     filesystem_handler part of the test.
//     The method will be passed received fileEntry object, original file
//     content, response received from filesystem_handler and callback
//     function that will expect error object as its argument (or undefined on
//     success).
// TODO(tbarzic): Rename this to TestParams, or something similar.
var TestExpectations = function(fileExtension, expectedTasks,
    fileVerifierFunction) {
  this.fileText_ = undefined;
  this.file_ = undefined;

  // TODO(tbarzic): Get rid of this.expectedTasks_, since it's not used anymore.
  this.expectedTasks_ = expectedTasks;
  this.fileExtension_ = fileExtension;
  this.fileVerifierFunction_ = fileVerifierFunction;
};

// This has to be called before verifyHandlerRequest.
TestExpectations.prototype.setFileAndFileText = function(file, fileText) {
  this.file_ = file;
  this.fileText_ = fileText;
};

TestExpectations.prototype.getFileExtension = function() {
  return this.fileExtension_;
};

TestExpectations.prototype.verifyHandlerRequest = function(request, callback) {
  if (!request) {
    callback({message: "Request from handler not defined."});
    return;
  }

  if (!request.fileContent) {
    var error = request.error || {message: "Undefined error."};
    callback(error);
    return;
  }

  if (!this.file_ || !this.fileText_ || !this.fileVerifierFunction_) {
    callback({message: "Test expectations not set properly."});
    return;
  }

  this.fileVerifierFunction_(this.file_, this.fileText_, request,
                             callback);
};

// Class that is in charge for running the test.
var TestRunner = function(expectations) {
  this.expectations_ = expectations;
  this.fileCreator_ = new TestFileCreator("tmp", true /* shouldRandomize */);
  this.listener_ = this.onHandlerRequest_.bind(this);
};

// Starts the test.
TestRunner.prototype.runTest = function() {
  // Get local FS, create dir with a file in it.
  console.log('Requesting local file system...');
  chrome.runtime.onMessageExternal.addListener(this.listener_);
  chrome.fileBrowserPrivate.requestLocalFileSystem(
      this.onFileSystemFetched_.bind(this));
};

TestRunner.prototype.onFileSystemFetched_ = function(fs) {
  if (!fs) {
    this.errorCallback_(chrome.extensions.lastError);
    return;
  }

  this.fileCreator_.init(fs, this.onFileCreatorInit_.bind(this),
                             this.errorCallback_.bind(this));
};

TestRunner.prototype.onFileCreatorInit_ = function() {
  var ext = this.expectations_.getFileExtension();
  if (!ext) {
    this.errorCallback_({message: "Test file extension not set."});
    return;
  }
  console.log(this.fileExtension);
  var self = this;
  this.fileCreator_.createFile('.log',
      function(file, text) {
        self.fileCreator_.createFile(ext,
            self.onFileCreated_.bind(self),
            self.errorCallback_.bind(self));
      },
      this.errorCallback_.bind(this));
};

TestRunner.prototype.onFileCreated_ = function(file, text) {
  // Start
  console.log('Get registered tasks now...');
  this.expectations_.setFileAndFileText(file, text);
  var fileUrl = file.toURL();

  chrome.fileBrowserPrivate.getFileTasks([fileUrl], [],
                                         this.onGetTasks_.bind(this, fileUrl));
};

TestRunner.prototype.onGetTasks_ = function(fileUrl, tasks) {
  console.log('Tasks: ');
  console.log(tasks);
  if (!tasks || !tasks.length) {
    this.errorCallback_({message: 'No tasks registered'});
    return;
  }

  console.log('DONE fetching ' + tasks.length + ' tasks');

  tasks = this.filterTasks_(tasks);
  chrome.fileBrowserPrivate.executeTask(tasks[0].taskId, [fileUrl]);
};

TestRunner.prototype.filterTasks_ = function(tasks) {
  var result = [];
  for (var i = 0; i < tasks.length; i++) {
    if (tasks[i].taskId.split('|')[0] != kFileManagerExtensionId) {
      result.push(tasks[i]);
    }
  }
  return result;
};

TestRunner.prototype.errorCallback_ = function(error) {
  var msg = '';
  if (!error.code) {
    msg = error.message;
  } else {
    switch (error.code) {
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

  this.fileCreator_.cleanupAndEndTest(
      this.reportFail_.bind(this, 'Got unexpected error: ' + msg),
      this.reportFail_.bind(this, 'Got unexpected error: ' + msg));
};

TestRunner.prototype.reportSuccess_ = function(entry) {
  chrome.test.succeed();
};

TestRunner.prototype.reportFail_ = function(message) {
  chrome.test.fail(message);
};

// Listens for the request from the filesystem_handler extension. When the
// event is received, it verifies it and stops listening for further events.
TestRunner.prototype.onHandlerRequest_ =
    function(request, sender, sendResponse) {
  this.expectations_.verifyHandlerRequest(
      request,
      this.verifyRequestCallback_.bind(this, sendResponse));
  chrome.runtime.onMessageExternal.removeListener(this.listener_);
};

TestRunner.prototype.verifyRequestCallback_ = function(sendResponse, error) {
  if (!error) {
    sendResponse({success: true});
    this.fileCreator_.cleanupAndEndTest(this.reportSuccess_.bind(this),
                                        this.reportFail_.bind(this,
                                                              cleanupError));
  } else {
    sendResponse({success: false});
    this.errorCallback_(error);
  }
};
