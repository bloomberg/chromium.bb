// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for the testing stuff.
 */
camera.test = {};

/**
 * WebSocket port to be used to communicate with the python test runner. Keep
 * in sync with tests/run_test.py.
 *
 * @type {number}
 * @const
 */
camera.test.WEBSOCKET_PORT = 47552;

/**
 * @type {Socket}
 * @private
 */
camera.test.socket_ = null;

/**
 * Verifies if the condition is true. In case of failure calls the onFailure
 * callback and returns false. Otherwise, returns true.
 *
 * @param {*} condition Condition to be evaluated.
 * @param {string} message Message to be shown.
 * @param {function()} onFailure Failure callback.
 * @return {boolean} True for passing, false for failing.
 */
camera.test.assert = function(condition, message, onFailure) {
  if (!condition) {
    camera.test.failure('Assertion error: ' + message);
    onFailure();
    return false;
  }

  return true;
};

/**
 * Run asynchronous steps sequentially.
 * @param {Array.<function(function())>} steps Array of steps.
 */
camera.test.runSteps = function(steps) {
  var serveNextStep = function() {
    if (steps.length) {
      var step = steps.shift();
      step(serveNextStep);
    }
  }
  serveNextStep();
};

/**
 * Calls the passed closure repeatedly until it returns true.
 * @param {string} message Message to be shown.
 * @param {function()} closure Closure to be repeated.
 * @param {function()} callback Success callback.
 */
camera.test.waitForTrue = function(message, closure, callback) {
  camera.test.info('Waiting for: ' + message);
  var check = function() {
    if (!closure())
      setTimeout(check, 100);
    else
      callback();
  };

  check();
};

/**
 * @param {string} name Command name.
 * @param {string=} opt_message Command message.
 */
camera.test.command = function(name, opt_message) {
  this.socket_.send(
      'COMMAND ' + name + (opt_message ? ' ' + opt_message : ''));
};

/**
 * @param {string=} opt_message Success message.
 */
camera.test.info = function(opt_message) {
  this.socket_.send('INFO' + (opt_message ? ' ' + opt_message : ''));
};

/**
 * @param {string=} opt_message Success message.
 */
camera.test.success = function(opt_message) {
  this.socket_.send('SUCCESS' + (opt_message ? ' ' + opt_message : ''));
};

/**
 * @param {string=} opt_message Failure message.
 */
camera.test.failure = function(opt_message) {
  this.socket_.send('FAILURE' + (opt_message ? ' ' + opt_message : ''));
};

/**
 * Initializes tests, by trying to connect to the testing server.
 */
camera.test.initialize = function() {
  camera.test.socket_ = new WebSocket(
      'ws://localhost:' + camera.test.WEBSOCKET_PORT);
  camera.test.socket_.onmessage = function(event) {
    var testCase = event.data;
    if (testCase in camera.test.cases) {
      camera.test.cases[testCase](function() {
        camera.test.success('Test case completed.');
        camera.test.socket_.close();
      });
    } else {
      camera.test.failure('Test case not found.');
      camera.test.socket_.close();
    }
  };
};

camera.test.initialize();
