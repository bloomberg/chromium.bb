// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * While FileOperationManager is run in the background page, this class is
 * used to communicate with it.
 * @param {DOMWindow} backgroundPage Window object of the background page.
 * @constructor
 */
function FileOperationManagerWrapper(backgroundPage) {
  this.fileOperationManager_ =
      backgroundPage.FileOperationManager.getInstance();
}

/**
 * Create a new instance or get existing instance of FCMW.
 * @param {DOMWindow} backgroundPage Window object of the background page.
 * @return {FileOperationManagerWrapper}  FileOperationManagerWrapper instance.
 */
FileOperationManagerWrapper.getInstance = function(backgroundPage) {
  if (!FileOperationManagerWrapper.instance_)
    FileOperationManagerWrapper.instance_ =
        new FileOperationManagerWrapper(backgroundPage);

  return FileOperationManagerWrapper.instance_;
};

/**
 * @return {boolean} True if there is a running task.
 */
FileOperationManagerWrapper.prototype.isRunning = function() {
  return this.fileOperationManager_.hasQueuedTasks();
};

/**
 * Decorates a FileOperationManager method, so it will be executed after
 * initializing the FileOperationManager instance in background page.
 * @param {string} method The method name.
 */
FileOperationManagerWrapper.decorateAsyncMethod = function(method) {
  FileOperationManagerWrapper.prototype[method] = function() {
    this.fileOperationManager_.willRunNewMethod();
    this.fileOperationManager_[method].apply(
        this.fileOperationManager_, arguments);
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
