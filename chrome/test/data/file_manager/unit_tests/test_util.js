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
 * Returns a callable object that records calls and the arguments of the
 * last call.
 *
 * @return {function()} A callable object (a function) with support methods
 * {@code assertCallCount(number)} and
 * {@code getLastArguments()} useful for making asserts and
 * inspecting information about recorded calls.
 */
function createRecordingFunction() {

  /** @type {!Array.<!Argument>} */
  var calls = [];

  function recorder() {
    calls.push(arguments);
  }

  /**
   * Asserts that the recorder was called {@code expected} times.
   * @param {number} expected The expected number of calls.
   */
  recorder.assertCallCount = function(expected) {
    var actual = calls.length;
    assertEquals(
        expected, actual,
        'Expected ' + expected + ' call(s), but was ' + actual + '.');
  };

  /**
   * @return {?Arguments} Returns the {@code Arguments} for the last call,
   *    or null if the recorder hasn't been called.
   */
  recorder.getLastArguments = function() {
    return (calls.length == 0) ?
        null :
        calls[calls.length - 1];
  };

  return recorder;
}
