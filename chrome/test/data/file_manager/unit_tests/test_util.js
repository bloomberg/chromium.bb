// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var INTERVAL_FOR_WAIT_UNTIL = 100; // ms

/**
 * Invokes a callback function depending on the result of promise.
 *
 * @param {Promise} promise Promise.
 * @param {function(boolean)} callback Callback function. True is passed if the
 *     test failed.
 */
function reportPromise(promise, callback) {
  promise.then(function() {
    callback(/* error */ false);
  }, function(error) {
    console.error(error.stack || error);
    callback(/* error */ true);
  });
}

/**
 * Waits until testFunction becomes true.
 * @param {function(): boolean} testFunction A function which is tested.
 * @return {!Promise} A promise which is fullfilled when the testFunction
 *     becomes true.
 */
function waitUntil(testFunction) {
  return new Promise(function(resolve) {
    var tryTestFunction = function() {
      if (testFunction())
        resolve();
      else
        setTimeout(tryTestFunction, INTERVAL_FOR_WAIT_UNTIL);
    };

    tryTestFunction();
  });
}

/**
 * A class that captures calls to a funtion so that values can be validated.
 * For use in tests only.
 *
 * <p>Example:
 * <pre>
 *   var recorder = new TestCallRecorder();
 *   someClass.addListener(recorder.callable);
 *   // do stuff ...
 *   recorder.assertCallCount(1);
 *   assertEquals(recorder.getListCall()[0], 'hammy');
 * </pre>
 */
TestCallRecorder = function() {
  /** @private {!Array.<!Argument>} */
  this.calls_ = [];

  /**
   * The recording funciton. Bound in our constructor to ensure we always
   * return the same object. This is necessary as some clients may make use
   * of object equality.
   *
   * @type {function()}
   */
  this.callback = this.recordArguments_.bind(this);
};

/**
 * Records the magic {@code arguments} value for later inspection.
 * @private
 */
TestCallRecorder.prototype.recordArguments_ = function() {
  this.calls_.push(arguments);
};

/**
 * Asserts that the recorder was called {@code expected} times.
 * @param {number} expected The expected number of calls.
 */
TestCallRecorder.prototype.assertCallCount = function(expected) {
  var actual = this.calls_.length;
  assertEquals(
      expected, actual,
      'Expected ' + expected + ' call(s), but was ' + actual + '.');
};

/**
 * @return {?Arguments} Returns the {@code Arguments} for the last call,
 *    or null if the recorder hasn't been called.
 */
TestCallRecorder.prototype.getLastArguments = function() {
  return (this.calls_.length === 0) ?
      null :
      this.calls_[this.calls_.length - 1];
};
