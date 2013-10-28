// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for async utility functions.
 */
var AsyncUtil = {};

/**
 * Asynchronous version of Array.forEach.
 * This executes a provided function callback once per array element, then
 * run completionCallback to notify the completion.
 * The callback can be an asynchronous function, but the execution is
 * sequentially done.
 *
 * @param {Array.<T>} array The array to be iterated.
 * @param {function(function(), T, number, Array.<T>} callback The iteration
 *     callback. The first argument is a callback to notify the completion of
 *     the iteration.
 * @param {function()} completionCallback Called when all iterations are
 *     completed.
 * @param {Object=} opt_thisObject Bound to callback if given.
 * @template T
 */
AsyncUtil.forEach = function(
    array, callback, completionCallback, opt_thisObject) {
  if (opt_thisObject)
    callback = callback.bind(opt_thisObject);

  var queue = new AsyncUtil.Queue();
  for (var i = 0; i < array.length; i++) {
    queue.run(function(element, index, iterationCompletionCallback) {
      callback(iterationCompletionCallback, element, index, array);
    }.bind(null, array[i], i));
  }
  queue.run(function(iterationCompletionCallback) {
    completionCallback();  // Don't pass iteration completion callback.
  });
};

/**
 * Creates a class for executing several asynchronous closures in a fifo queue.
 * Added tasks will be executed sequentially in order they were added.
 *
 * @constructor
 */
AsyncUtil.Queue = function() {
  this.running_ = false;
  this.closures_ = [];
};

/**
 * @return {boolean} True when a task is running, otherwise false.
 */
AsyncUtil.Queue.prototype.isRunning = function() {
  return this.running_;
};

/**
 * Enqueues a closure to be executed.
 * @param {function(function())} closure Closure with a completion callback to
 *     be executed.
 */
AsyncUtil.Queue.prototype.run = function(closure) {
  this.closures_.push(closure);
  if (!this.running_)
    this.continue_();
};

/**
 * Serves the next closure from the queue.
 * @private
 */
AsyncUtil.Queue.prototype.continue_ = function() {
  if (!this.closures_.length) {
    this.running_ = false;
    return;
  }

  // Run the next closure.
  this.running_ = true;
  var closure = this.closures_.shift();
  closure(this.continue_.bind(this));
};

/**
 * Cancels all pending tasks. Note that this does NOT cancel the task running
 * currently.
 */
AsyncUtil.Queue.prototype.cancel = function() {
  this.closures_ = [];
};

/**
 * Creates a class for executing several asynchronous closures in a group in
 * a dependency order.
 *
 * @constructor
 */
AsyncUtil.Group = function() {
  this.addedTasks_ = {};
  this.pendingTasks_ = {};
  this.finishedTasks_ = {};
  this.completionCallbacks_ = [];
};

/**
 * Enqueues a closure to be executed after dependencies are completed.
 *
 * @param {function(function())} closure Closure with a completion callback to
 *     be executed.
 * @param {Array.<string>=} opt_dependencies Array of dependencies. If no
 *     dependencies, then the the closure will be executed immediately.
 * @param {string=} opt_name Task identifier. Specify to use in dependencies.
 */
AsyncUtil.Group.prototype.add = function(closure, opt_dependencies, opt_name) {
  var length = Object.keys(this.addedTasks_).length;
  var name = opt_name || ('(unnamed#' + (length + 1) + ')');

  var task = {
    closure: closure,
    dependencies: opt_dependencies || [],
    name: name
  };

  this.addedTasks_[name] = task;
  this.pendingTasks_[name] = task;
};

/**
 * Runs the enqueued closured in order of dependencies.
 *
 * @param {function()=} opt_onCompletion Completion callback.
 */
AsyncUtil.Group.prototype.run = function(opt_onCompletion) {
  if (opt_onCompletion)
    this.completionCallbacks_.push(opt_onCompletion);
  this.continue_();
};

/**
 * Runs enqueued pending tasks whose dependencies are completed.
 * @private
 */
AsyncUtil.Group.prototype.continue_ = function() {
  // If all of the added tasks have finished, then call completion callbacks.
  if (Object.keys(this.addedTasks_).length ==
      Object.keys(this.finishedTasks_).length) {
    for (var index = 0; index < this.completionCallbacks_.length; index++) {
      var callback = this.completionCallbacks_[index];
      callback();
    }
    this.completionCallbacks_ = [];
    return;
  }

  for (var name in this.pendingTasks_) {
    var task = this.pendingTasks_[name];
    var dependencyMissing = false;
    for (var index = 0; index < task.dependencies.length; index++) {
      var dependency = task.dependencies[index];
      // Check if the dependency has finished.
      if (!this.finishedTasks_[dependency])
        dependencyMissing = true;
    }
    // All dependences finished, therefore start the task.
    if (!dependencyMissing) {
      delete this.pendingTasks_[task.name];
      task.closure(this.finish_.bind(this, task));
    }
  }
};

/**
 * Finishes the passed task and continues executing enqueued closures.
 *
 * @param {Object} task Task object.
 * @private
 */
AsyncUtil.Group.prototype.finish_ = function(task) {
  this.finishedTasks_[task.name] = task;
  this.continue_();
};

/**
 * Aggregates consecutive calls and executes the closure only once instead of
 * several times. The first call is always called immediately, and the next
 * consecutive ones are aggregated and the closure is called only once once
 * |delay| amount of time passes after the last call to run().
 *
 * @param {function()} closure Closure to be aggregated.
 * @param {number=} opt_delay Minimum aggregation time in milliseconds. Default
 *     is 50 milliseconds.
 * @constructor
 */
AsyncUtil.Aggregation = function(closure, opt_delay) {
  /**
   * @type {number}
   * @private
   */
  this.delay_ = opt_delay || 50;

  /**
   * @type {function()}
   * @private
   */
  this.closure_ = closure;

  /**
   * @type {number?}
   * @private
   */
  this.scheduledRunsTimer_ = null;

  /**
   * @type {number}
   * @private
   */
  this.lastRunTime_ = 0;
};

/**
 * Runs a closure. Skips consecutive calls. The first call is called
 * immediately.
 */
AsyncUtil.Aggregation.prototype.run = function() {
  // If recently called, then schedule the consecutive call with a delay.
  if (Date.now() - this.lastRunTime_ < this.delay_) {
    this.cancelScheduledRuns_();
    this.scheduledRunsTimer_ = setTimeout(this.runImmediately_.bind(this),
                                          this.delay_ + 1);
    this.lastRunTime_ = Date.now();
    return;
  }

  // Otherwise, run immediately.
  this.runImmediately_();
};

/**
 * Calls the schedule immediately and cancels any scheduled calls.
 * @private
 */
AsyncUtil.Aggregation.prototype.runImmediately_ = function() {
  this.cancelScheduledRuns_();
  this.closure_();
  this.lastRunTime_ = Date.now();
};

/**
 * Cancels all scheduled runs (if any).
 * @private
 */
AsyncUtil.Aggregation.prototype.cancelScheduledRuns_ = function() {
  if (this.scheduledRunsTimer_) {
    clearTimeout(this.scheduledRunsTimer_);
    this.scheduledRunsTimer_ = null;
  }
};
