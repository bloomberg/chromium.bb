/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * The callback to send test messages to. By default we will assume that we
 * are being run by an automated test case such as a browser test, but this can
 * be overridden.
 * @private
 */
var gReturnCallback = sendToTest;

/**
 * The callback to send debug messages to. By default we assume console.log.
 * @private
 */
var gDebugCallback = consoleLog_;

/**
 * Replaces the test message callback. Test messages are messages sent by the
 * returnToTest function.
 *
 * @param callback A function that takes a single string (the message).
 */
function replaceReturnCallback(callback) {
  gReturnCallback = callback;
}

/**
 * Replaces the debug message callback. Debug messages are messages sent by the
 * debug function.
 *
 * @param callback A function that takes a single string (the message).
 */
function replaceDebugCallback(callback) {
  gDebugCallback = callback;
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

  gDebugCallback(prefix + txt);
}

/**
 * Sends a value back to the test.
 *
 * @param {string} message The message to return.
 */
function returnToTest(message) {
  gReturnCallback(message);
}

/**
 * Sends a message to the test case. Requires that this javascript was
 * loaded by the test. This will make the test proceed if it is blocked in a
 * ExecuteJavascript call.
 *
 * @param {string} message The message to send.
 */
function sendToTest(message) {
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
  console.error(reason);
  sendToTest('Test failed: ' + reason);
  return new Error(reason);
}

/** @private */
function consoleLog_(message) {
  // It is not legal to treat console.log as a first-class object, so wrap it.
  console.log(message);
}
