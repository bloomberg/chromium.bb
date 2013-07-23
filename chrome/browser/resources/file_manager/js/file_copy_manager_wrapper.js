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
  this.running_ = false;

  var onEventBound = this.onEvent_.bind(this);
  chrome.runtime.getBackgroundPage(function(backgroundPage) {
    var fileCopyManager = backgroundPage.FileCopyManager.getInstance();
    fileCopyManager.addListener(onEventBound);

    // Register removing the listener when the window is closed.
    chrome.app.window.current().onClosed.addListener(function() {
      fileCopyManager.removeListener(onEventBound);
    });
  });
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
 * Called be FileCopyManager to raise an event in this instance of FileManager.
 * @param {string} eventName Event name.
 * @param {Object} eventArgs Arbitratry field written to event object.
 * @private
 */
FileCopyManagerWrapper.prototype.onEvent_ = function(eventName, eventArgs) {
  this.running_ = eventArgs.status.totalFiles > 0;

  var event = new cr.Event(eventName);
  for (var arg in eventArgs)
    if (eventArgs.hasOwnProperty(arg))
      event[arg] = eventArgs[arg];

  this.dispatchEvent(event);
};

/**
 * @return {boolean} True if there is a running task.
 */
FileCopyManagerWrapper.prototype.isRunning = function() {
  return this.running_;
};

/**
 * Load background page and call callback with copy manager as an argument.
 * @param {function} callback Function with FileCopyManager as a parameter.
 * @private
 */
FileCopyManagerWrapper.prototype.getCopyManagerAsync_ = function(callback) {
  chrome.runtime.getBackgroundPage(function(backgroundPage) {
    var fileCopyManager = backgroundPage.FileCopyManager.getInstance();
    fileCopyManager.initialize(callback.bind(this, fileCopyManager));
  });
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
