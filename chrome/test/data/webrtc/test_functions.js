/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * This list keeps track of failures in the test. This is necessary to keep
 * track of failures that happen outside of PyAuto test calls and need to be
 * reported asynchronously.
 * @private
 */
var gFailures = [];

/**
 * @return {string} Returns either the string ok-no-errors or a text message
 *     which describes the failure(s) which have been registered through calls
 *     to addTestFailure in this source file.
 */
function getAnyTestFailures() {
  if (gFailures.length == 1)
    returnToPyAuto('Test failure: ' + gFailures[0]);
  else if (gFailures.length > 1)
    returnToPyAuto('Multiple failures: ' + gFailures.join(' AND '));
  else
    returnToPyAuto('ok-no-errors');
}

// Helper / error handling functions.

/**
 * Prints a debug message on the webpage itself.
 */
function debug(txt) {
  if (gOurClientName == null)
    prefix = '';
  else
    prefix = gOurClientName + ' says: ';

  console.log(prefix + txt)
}

/**
 * Sends a value back to the PyAuto test. This will make the test proceed if it
 * is blocked in a ExecuteJavascript call.
 * @param {string} message The message to return.
 */
function returnToPyAuto(message) {
  debug('Returning ' + message + ' to PyAuto.');
  window.domAutomationController.send(message);
}

/**
 * Adds a test failure without affecting the control flow. If the PyAuto test is
 * blocked, this function will immediately break that call with an error
 * message. Otherwise, the error is saved and it is up to the test to check it
 * with getAnyTestFailures.
 *
 * @param {string} reason The reason why the test failed.
 */
function addTestFailure(reason) {
  returnToPyAuto('Test failed: ' + reason)
  gFailures.push(reason);
}

/**
 * Follows the same contract as addTestFailure but also throws an exception to
 * stop the control flow locally.
 */
function failTest(reason) {
  addTestFailure(reason);
  throw new Error(reason);
}