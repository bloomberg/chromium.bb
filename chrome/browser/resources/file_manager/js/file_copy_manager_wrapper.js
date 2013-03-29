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
  this.status_ = {
    pendingItems: 0,
    pendingFiles: 0,
    pendingDirectories: 0,
    pendingBytes: 0,

    completedItems: 0,
    completedFiles: 0,
    completedDirectories: 0,
    completedBytes: 0,

    totalItems: 0,
    totalFiles: 0,
    totalDirectories: 0,
    totalBytes: 0,

    percentage: NaN,
    pendingCopies: 0,
    pendingMoves: 0,
    filename: ''  // In case pendingItems == 1
  };
}

/**
 * Extending cr.EventTarget.
 */
FileCopyManagerWrapper.prototype.__proto__ = cr.EventTarget.prototype;

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
 * Load background page and call callback with copy manager as an argument.
 * @param {function} callback Function with FileCopyManager as a parameter.
 * @private
 */
FileCopyManagerWrapper.prototype.getCopyManagerAsync_ = function(callback) {
  var MAX_RETRIES = 10;
  var TIMEOUT = 100;

  var retries = 0;

  var tryOnce = function() {
    var onGetBackgroundPage = function(bg) {
      if (bg) {
        var fileCopyManager = bg.FileCopyManager.getInstance();
        fileCopyManager.initialize(callback.bind(this, fileCopyManager));
        return;
      }
      if (++retries < MAX_RETRIES)
        setTimeout(tryOnce, TIMEOUT);
      else
        console.error('Can\'t get copy manager.');
    };
    if (chrome.runtime && chrome.runtime.getBackgroundPage)
      chrome.runtime.getBackgroundPage(onGetBackgroundPage);
    else
      onGetBackgroundPage(chrome.extension.getBackgroundPage());
  };

  tryOnce();
};

/**
 * Called be FileCopyManager to raise an event in this instance of FileManager.
 * @param {string} eventName Event name.
 * @param {Object} eventArgs Arbitratry field written to event object.
 */
FileCopyManagerWrapper.prototype.onEvent = function(eventName, eventArgs) {
  this.status_ = eventArgs.status;

  var event = new cr.Event(eventName);
  for (var arg in eventArgs)
    if (eventArgs.hasOwnProperty(arg))
      event[arg] = eventArgs[arg];

  this.dispatchEvent(event);
};

/**
 * @return {Object} Status object.
 */
FileCopyManagerWrapper.prototype.getStatus = function() {
  return this.status_;
};

/**
 * Decorates a FileCopyManager method, so it will be executed after initializing
 * the FileCopyManager instance in background page.
 * @param {string} method The method name.
 */
FileCopyManagerWrapper.decorateAsyncMethod = function(method) {
  FileCopyManagerWrapper.prototype[method] = function() {
    var args = Array.prototype.slice.call(arguments);
    this.getCopyManagerAsync_(function(cm) {
      cm.willRunNewMethod();
      cm[method].apply(cm, args);
    });
  };
};

FileCopyManagerWrapper.decorateAsyncMethod('requestCancel');
FileCopyManagerWrapper.decorateAsyncMethod('paste');
FileCopyManagerWrapper.decorateAsyncMethod('deleteEntries');
FileCopyManagerWrapper.decorateAsyncMethod('forceDeleteTask');
FileCopyManagerWrapper.decorateAsyncMethod('cancelDeleteTask');
FileCopyManagerWrapper.decorateAsyncMethod('zipSelection');
