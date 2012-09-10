// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var fileCopyManagerWrapper = null;

/**
 * While FileCopyManager is run in the background page, this class is used to
 * communicate with it.
 * @constructor
 * @param {DirectoryEntry} root Root directory entry.
 */
function FileCopyManagerWrapper(root) {
  this.root_ = root;
}

/**
 * Extending cr.EventTarget.
 */
FileCopyManagerWrapper.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Create a new instance or get existing instance of FCMW.
 * @param {DirectoryEntry} root Root directory entry.
 * @return {FileCopyManagerWrapper}  A FileCopyManagerWrapper instance.
 */
FileCopyManagerWrapper.getInstance = function(root) {
  if (fileCopyManagerWrapper === null) {
    fileCopyManagerWrapper = new FileCopyManagerWrapper(root);
  }
  return fileCopyManagerWrapper;
};

/**
 * @private
 * @return {FileCopyManager?} Copy manager instance in the background page or
 *     NULL if background page is not loaded.
 */
FileCopyManagerWrapper.prototype.getCopyManagerSync_ = function() {
  var bg = chrome.extension.getBackgroundPage();
  if (!bg)
    return null;
  return bg.FileCopyManager.getInstance(this.root_);
};

/**
 * Load background page and call callback with copy manager as an argument.
 * @private
 * @param {Function} callback Function with FileCopyManager as a parameter.
 */
FileCopyManagerWrapper.prototype.getCopyManagerAsync_ = function(callback) {
  var MAX_RETRIES = 10;
  var TIMEOUT = 100;

  var root = this.root_;
  var retries = 0;

  function tryOnce() {
    chrome.runtime.getBackgroundPage(function(bg) {
      if (bg) {
        callback(bg.FileCopyManager.getInstance(root));
        return;
      }
      if (++retries < MAX_RETRIES)
        setTimeout(tryOnce, TIMEOUT);
    });
  }

  tryOnce();
};

/**
 * Called be FileCopyManager to raise an event in this instance of FileManager.
 * @param {string} eventName Event name.
 * @param {Object} eventArgs Arbitratry field written to event object.
 */
FileCopyManagerWrapper.prototype.onEvent = function(eventName, eventArgs) {
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
  var cm = this.getCopyManagerSync_();
  if (cm)
    return cm.getStatus();

  return {
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
      cm[method].apply(cm, args);
    });
  };
};

FileCopyManagerWrapper.decorateAsyncMethod('requestCancel');
FileCopyManagerWrapper.decorateAsyncMethod('paste');
FileCopyManagerWrapper.decorateAsyncMethod('deleteEntries');
FileCopyManagerWrapper.decorateAsyncMethod('forceDeleteTask');
FileCopyManagerWrapper.decorateAsyncMethod('cancelDeleteTask');
