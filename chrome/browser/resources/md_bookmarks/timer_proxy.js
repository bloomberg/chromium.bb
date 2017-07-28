// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A class that mediates window timer calls to support mockability
 * in tests.
 */

cr.define('bookmarks', function() {
  /** @constructor */
  function TimerProxy() {}

  TimerProxy.prototype = {
    /**
     * @param {function()|string} fn
     * @param {number=} delay
     * @return {number}
     */
    setTimeout: function(fn, delay) {
      return window.setTimeout(fn, delay);
    },

    /** @param {number|undefined?} id */
    clearTimeout: function(id) {
      window.clearTimeout(id);
    },
  };

  /**
   * A one-shot debouncer which fires the given callback after a delay. The
   * delay can be refreshed by calling resetTimer. Resetting with no delay moves
   * the callback to the end of the task queue.
   * @param {!function()} callback
   * @constructor
   */
  function Debouncer(callback) {
    /** @private {!function()} */
    this.callback_ = callback;
    /** @private {!bookmarks.TimerProxy} */
    this.timerProxy_ = new TimerProxy();
    /** @private {?number} */
    this.timer_ = null;
    /** @private {!function()} */
    this.boundTimerCallback_ = this.timerCallback_.bind(this);
    /** @private {boolean} */
    this.isDone_ = false;
    /** @private {!PromiseResolver} */
    this.promiseResolver_ = new PromiseResolver();
  }

  Debouncer.prototype = {
    /**
     * @param {number=} delay
     */
    resetTimeout: function(delay) {
      if (this.timer_)
        this.timerProxy_.clearTimeout(this.timer_);
      this.timer_ =
          this.timerProxy_.setTimeout(this.boundTimerCallback_, delay);
    },

    /**
     * @return {boolean} True if the Debouncer has finished processing.
     */
    done: function() {
      return this.isDone_;
    },

    /**
     * @return {Promise} Promise which resolves immediately after the callback.
     */
    get promise() {
      return this.promiseResolver_.promise;
    },

    /** @private */
    timerCallback_: function() {
      this.isDone_ = true;
      this.callback_.call();
      this.promiseResolver_.resolve();
    },
  };

  return {
    Debouncer: Debouncer,
    TimerProxy: TimerProxy,
  };
});
