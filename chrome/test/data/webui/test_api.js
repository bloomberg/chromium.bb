// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library providing basic test framework functionality.

(function() {
  // Indicates a pass to the C++ backend.
  function pass(message) {
    chrome.send('Pass', []);
  }

  // Indicates a fail to the C++ backend.
  function fail(message) {
    chrome.send('Fail', []);
  }

  // Asserts.
  // Use the following assertions to verify a condition within a test.
  // If assertion fails, the C++ backend will be immediately notified.
  // If assertion passes, no notification will be sent to the C++ backend.
  function assertBool(test, expected, message) {
    if (test !== expected) {
      if (message)
        message = test + '\n' + message;
      else
        message = test;
      fail(message);
    }
  }

  function assertTrue(test, message) {
    assertBool(test, true, message);
  }

  function assertFalse(test, message) {
    assertBool(test, false, message);
  }

  function assertEquals(expected, actual, message) {
    if (expected !== actual) {
      fail('Test Error in ' + testName(currentTest) +
           '\nActual: ' + actual + '\nExpected: ' + expected + '\n' + message);
    }
    if (typeof expected != typeof actual) {
      fail('Test Error in ' + testName(currentTest) +
           ' (type mismatch)\nActual Type: ' + typeof actual +
           '\nExpected Type:' + typeof expected + '\n' + message);
    }
  }

  function assertNotReached(message) {
    fail(message);
  }

  // Call this method within your test script file to begin tests.
  // Takes an array of functions; each function is a test.
  function runTests(tests) {
    var currentTest = tests.shift();

    while (currentTest) {
      try {
        console.log('Running test ' + currentTest.name);
        currentTest.call();
        currentTest = tests.shift();
      } catch (e) {
        console.log(
            'Failed: ' + currentTest.name + '\nwith exception: ' + e.message);

        fail();
      }
    }

    // All tests passed.
    pass('');
  }

  // Exports.
  window.assertTrue = assertTrue;
  window.assertFalse = assertFalse;
  window.assertEquals = assertEquals;
  window.assertNotReached = assertNotReached;
  window.runTests = runTests;
})();