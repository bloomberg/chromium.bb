// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @class FunctionSequence to invoke steps in sequence
 *
 * @param steps             array of functions to invoke in parallel
 * @param callback          callback to invoke on success
 * @param failureCallback   callback to invoke on failure
 */
function FunctionParallel(name, steps, logger, callback, failureCallback) {
  // Private variables hidden in closure
  this.currentStepIdx_ = -1;
  this.failed_ = false;
  this.steps_ = steps;
  this.callback_ = callback;
  this.failureCallback_ = failureCallback;
  this.logger = logger;
  this.name = name;

  this.remaining = this.steps_.length;

  this.nextStep = this.nextStep_.bind(this);
  this.onError = this.onError_.bind(this);
  this.apply = this.start.bind(this);
}


/**
 * Error handling function, which fires error callback.
 *
 * @param err error message
 */
FunctionParallel.prototype.onError_ = function(err) {
  if (!this.failed_) {
    this.failed_ = true;
    this.failureCallback_(err);
  }
};

/**
 * Advances to next step. This method should not be used externally. In external
 * cases should be used nextStep function, which is defined in closure and thus
 * has access to internal variables of functionsequence.
 */
FunctionParallel.prototype.nextStep_ = function() {
  if (--this.remaining == 0 && !this.failed_) {
    this.callback_();
  }
};

/**
 * This function should be called only once on start, so start all the children
 * at once
 */
FunctionParallel.prototype.start = function(var_args) {
  this.logger.vlog('Starting [' + this.steps_.length + '] parallel tasks with '
                    + arguments.length + ' argument(s)');
  if (this.logger.verbose) {
    for (var j = 0; j < arguments.length; j++) {
      this.logger.vlog(arguments[j]);
    }
  }
  for (var i=0; i < this.steps_.length; i++) {
    this.logger.vlog('Attempting to start step [' + this.steps_[i].name + ']');
    try {
      this.steps_[i].apply(this, arguments);
    } catch(e) {
      this.onError(e.toString());
    }
  }
};
