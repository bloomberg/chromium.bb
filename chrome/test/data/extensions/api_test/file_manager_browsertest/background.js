// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Extension ID of Files.app.
 * @type {string}
 * @const
 */
var FILE_MANAGER_EXTENSIONS_ID = 'hhaomjibdihmijegdhdafkllkbggdgoj';

/**
 * Calls a remote test util in Files.app's extension. See: test_util.js.
 *
 * @param {string} func Function name.
 * @param {?string} appId Target window's App ID or null for functions
 *     not requiring a window.
 * @param {Array.<*>} args Array of arguments.
 * @param {function(*)} callback Callback handling the function's result.
 */
function callRemoteTestUtil(func, appId, args, callback) {
  chrome.runtime.sendMessage(
      FILE_MANAGER_EXTENSIONS_ID, {
        func: func,
        appId: appId,
        args: args
      },
      callback);
}

chrome.test.runTests([
  // Waits for the C++ code to send a string identifying a test, then runs that
  // test.
  function testRunner() {
    var command = chrome.extension.inIncognitoContext ? 'which test guest' :
        'which test non-guest';
    chrome.test.sendMessage(command, function(testCaseName) {
      // Run one of the test cases defined in the testcase namespace, in
      // test_cases.js. The test case name is passed via StartTest call in
      // file_manager_browsertest.cc.
      if (testcase[testCaseName])
        testcase[testCaseName]();
      else
        chrome.test.fail('Bogus test name passed to testRunner()');
    });
  }
]);
