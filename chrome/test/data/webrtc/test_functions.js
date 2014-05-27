/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Helper / error handling functions.

/**
 * Prints a debug message.
 */
function debug(txt) {
  console.log(txt);
}

/**
 * Sends a value back to the test.
 *
 * @param {string} message The message to return.
 */
function returnToTest(message) {
  debug('Returning ' + message + ' to test.');
  window.domAutomationController.send(message);
}

/**
 * Fails the test by generating an exception. If the test automation is calling
 * into us, make sure to fail the test as fast as possible. You must use this
 * function like this:
 *
 * throw failTest('my reason');
 *
 * @return {!Error}
 */
function failTest(reason) {
  var error = new Error(reason);
  returnToTest('Test failed: ' + error.stack);
  return error;
}
