// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * While FileCopyManager is run in the background page, this class is used to
 * communicate with it.
 * @constructor
 */
function FileCopyManagerWrapper() {
  this.fileCopyManager_ = null;

  /**
   * In the constructor, it tries to start loading the background page, and
   * its FileCopyManager instance, asynchronously. Even during the
   * initialization, there may be some method invocation. In such a case,
   * FileCopyManagerWrapper keeps it as a pending task in pendingTasks_,
   * and once FileCopyManager is obtained, run the pending tasks in the order.
   *
   * @private {Array.<function(FileCopyManager)>}
   */
  this.pendingTasks_ = [];

  chrome.runtime.getBackgroundPage(function(backgroundPage) {
    var fileCopyManager = backgroundPage.FileCopyManager.getInstance();
    fileCopyManager.initialize(function() {
      // Here the fileCopyManager is initialized. Keep the instance, and run
      // pending tasks.
      this.fileCopyManager_ = fileCopyManager;
      for (var i = 0; i < this.pendingTasks_.length; i++) {
        this.pendingTasks_[i](fileCopyManager);
      }
      this.pendingTasks_ = [];
    }.bind(this));
  }.bind(this));
}

/**
 * Create a new instance or get existing instance of FCMW.
 * @return {FileCopyManagerWrapper}  A FileCopyManagerWrapper instance.
 */
FileCopyManagerWrapper.getInstance = function() {
  if (!FileCopyManagerWrapper.instance_)
    FileCopyManagerWrapper.instance_ = new FileCopyManagerWrapper();

  return FileCopyManagerWrapper.instance_;
};

/**
 * @return {boolean} True if there is a running task.
 */
FileCopyManagerWrapper.prototype.isRunning = function() {
  // Note: until the background page is loaded, this method returns false
  // even if there is a running task in FileCopyManager. (Though the period
  // should be very short).
  // TODO(hidehiko): Fix the race condition. It is necessary to change the
  // FileCopyManager implementation as well as the clients (FileManager/
  // ButterBar) implementation.
  return this.fileCopyManager_ && this.fileCopyManager_.hasQueuedTasks();
};

/**
 * Decorates a FileCopyManager method, so it will be executed after initializing
 * the FileCopyManager instance in background page.
 * @param {string} method The method name.
 */
FileCopyManagerWrapper.decorateAsyncMethod = function(method) {
  FileCopyManagerWrapper.prototype[method] = function() {
    var args = Array.prototype.slice.call(arguments);
    var operation = function(fileCopyManager) {
      fileCopyManager.willRunNewMethod();
      fileCopyManager[method].apply(fileCopyManager, args);
    };
    if (this.fileCopyManager_)
      operation(this.fileCopyManager_);
    else
      this.pendingTasks_.push(operation);
  };
};

FileCopyManagerWrapper.decorateAsyncMethod('requestCancel');
FileCopyManagerWrapper.decorateAsyncMethod('paste');
FileCopyManagerWrapper.decorateAsyncMethod('deleteEntries');
FileCopyManagerWrapper.decorateAsyncMethod('forceDeleteTask');
FileCopyManagerWrapper.decorateAsyncMethod('cancelDeleteTask');
FileCopyManagerWrapper.decorateAsyncMethod('zipSelection');
FileCopyManagerWrapper.decorateAsyncMethod('addEventListener');
FileCopyManagerWrapper.decorateAsyncMethod('removeEventListener');
