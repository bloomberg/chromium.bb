// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * While FileOperationManager is run in the background page, this class is
 * used to communicate with it.
 * @constructor
 */
function FileOperationManagerWrapper() {
  this.fileOperationManager_ = null;

  /**
   * In the constructor, it tries to start loading the background page, and
   * its FileOperationManager instance, asynchronously. Even during the
   * initialization, there may be some method invocation. In such a case,
   * FileOperationManagerWrapper keeps it as a pending task in pendingTasks_,
   * and once FileOperationManager is obtained, run the pending tasks in the
   * order.
   *
   * @private {Array.<function(FileOperationManager)>}
   */
  this.pendingTasks_ = [];

  chrome.runtime.getBackgroundPage(function(backgroundPage) {
    this.fileOperationManager_ =
        backgroundPage.FileOperationManager.getInstance();

    // Run pending tasks.
    for (var i = 0; i < this.pendingTasks_.length; i++) {
      this.pendingTasks_[i](this.fileOperationManager_);
    }
    this.pendingTasks_ = [];
  }.bind(this));
}

/**
 * Create a new instance or get existing instance of FCMW.
 * @return {FileOperationManagerWrapper}  FileOperationManagerWrapper instance.
 */
FileOperationManagerWrapper.getInstance = function() {
  if (!FileOperationManagerWrapper.instance_)
    FileOperationManagerWrapper.instance_ = new FileOperationManagerWrapper();

  return FileOperationManagerWrapper.instance_;
};

/**
 * @return {boolean} True if there is a running task.
 */
FileOperationManagerWrapper.prototype.isRunning = function() {
  // Note: until the background page is loaded, this method returns false
  // even if there is a running task in FileOperationManager. (Though the
  // period should be very short).
  // TODO(hidehiko): Fix the race condition. It is necessary to change the
  // FileOperationManager implementation as well as the clients (FileManager/
  // ButterBar) implementation.
  return this.fileOperationManager_ &&
         this.fileOperationManager_.hasQueuedTasks();
};

/**
 * Decorates a FileOperationManager method, so it will be executed after
 * initializing the FileOperationManager instance in background page.
 * @param {string} method The method name.
 */
FileOperationManagerWrapper.decorateAsyncMethod = function(method) {
  FileOperationManagerWrapper.prototype[method] = function() {
    var args = Array.prototype.slice.call(arguments);
    var operation = function(fileOperationManager) {
      fileOperationManager.willRunNewMethod();
      fileOperationManager[method].apply(fileOperationManager, args);
    };
    if (this.fileOperationManager_)
      operation(this.fileOperationManager_);
    else
      this.pendingTasks_.push(operation);
  };
};

FileOperationManagerWrapper.decorateAsyncMethod('requestCancel');
FileOperationManagerWrapper.decorateAsyncMethod('paste');
FileOperationManagerWrapper.decorateAsyncMethod('deleteEntries');
FileOperationManagerWrapper.decorateAsyncMethod('forceDeleteTask');
FileOperationManagerWrapper.decorateAsyncMethod('cancelDeleteTask');
FileOperationManagerWrapper.decorateAsyncMethod('zipSelection');
FileOperationManagerWrapper.decorateAsyncMethod('addEventListener');
FileOperationManagerWrapper.decorateAsyncMethod('removeEventListener');
