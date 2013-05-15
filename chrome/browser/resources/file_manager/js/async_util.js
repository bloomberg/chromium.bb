// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for async utility functions.
 */
var AsyncUtil = {};

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
