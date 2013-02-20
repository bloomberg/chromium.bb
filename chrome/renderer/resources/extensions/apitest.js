// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// extension_apitest.js
// mini-framework for ExtensionApiTest browser tests

  chrome.test = chrome.test || {};

  chrome.test.tests = chrome.test.tests || [];

  var currentTest = null;
  var lastTest = null;
  var testsFailed = 0;
  var testCount = 1;
  var failureException = 'chrome.test.failure';

  // Helper function to get around the fact that function names in javascript
  // are read-only, and you can't assign one to anonymous functions.
  function testName(test) {
    return test ? (test.name || test.generatedName) : "(no test)";
  }

  function testDone() {
    // Use setTimeout here to allow previous test contexts to be
    // eligible for garbage collection.
    setTimeout(chrome.test.runNextTest, 0);
  }

  function allTestsDone() {
    if (testsFailed == 0) {
      chrome.test.notifyPass();
    } else {
      chrome.test.notifyFail('Failed ' + testsFailed + ' of ' +
                             testCount + ' tests');
    }

    // Try to get the script to stop running immediately.
    // This isn't an error, just an attempt at saying "done".
    throw "completed";
  }

  var pendingCallbacks = 0;

  chrome.test.callbackAdded = function() {
    pendingCallbacks++;

    var called = false;
    return function() {
      chrome.test.assertFalse(called, 'callback has already been run');
      called = true;

      pendingCallbacks--;
      if (pendingCallbacks == 0) {
        chrome.test.succeed();
      }
    };
  };

  chrome.test.runNextTest = function() {
    // There may have been callbacks which were interrupted by failure
    // exceptions.
    pendingCallbacks = 0;

    lastTest = currentTest;
    currentTest = chrome.test.tests.shift();

    if (!currentTest) {
      allTestsDone();
      return;
    }

    try {
      chrome.test.log("( RUN      ) " + testName(currentTest));
      currentTest.call();
    } catch (e) {
      if (e !== failureException)
        chrome.test.fail('uncaught exception: ' + e);
    }
  };

  chrome.test.fail = function(message) {
    chrome.test.log("(  FAILED  ) " + testName(currentTest));

    var stack = {};
    Error.captureStackTrace(stack, chrome.test.fail);

    if (!message)
      message = "FAIL (no message)";

    message += "\n" + stack.stack;
    console.log("[FAIL] " + testName(currentTest) + ": " + message);
    testsFailed++;
    testDone();

    // Interrupt the rest of the test.
    throw failureException;
  };

  chrome.test.succeed = function() {
    console.log("[SUCCESS] " + testName(currentTest));
    chrome.test.log("(  SUCCESS )");
    testDone();
  };

  chrome.test.assertTrue = function(test, message) {
    chrome.test.assertBool(test, true, message);
  };

  chrome.test.assertFalse = function(test, message) {
    chrome.test.assertBool(test, false, message);
  };

  chrome.test.assertBool = function(test, expected, message) {
    if (test !== expected) {
      if (typeof(test) == "string") {
        if (message)
          message = test + "\n" + message;
        else
          message = test;
      }
      chrome.test.fail(message);
    }
  };

  chrome.test.checkDeepEq = function (expected, actual) {
    if ((expected === null) != (actual === null))
      return false;

    if (expected === actual)
      return true;

    if (typeof(expected) !== typeof(actual))
      return false;

    for (var p in actual) {
      if (actual.hasOwnProperty(p) && !expected.hasOwnProperty(p))
        return false;
    }
    for (var p in expected) {
      if (expected.hasOwnProperty(p) && !actual.hasOwnProperty(p))
        return false;
    }

    for (var p in expected) {
      var eq = true;
      switch (typeof(expected[p])) {
        case 'object':
          eq = chrome.test.checkDeepEq(expected[p], actual[p]);
          break;
        case 'function':
          eq = (typeof(actual[p]) != 'undefined' &&
                expected[p].toString() == actual[p].toString());
          break;
        default:
          eq = (expected[p] == actual[p] &&
                typeof(expected[p]) == typeof(actual[p]));
          break;
      }
      if (!eq)
        return false;
    }
    return true;
  };

  chrome.test.assertEq = function(expected, actual, message) {
    var error_msg = "API Test Error in " + testName(currentTest);
    if (message)
      error_msg += ": " + message;
    if (typeof(expected) == 'object') {
      if (!chrome.test.checkDeepEq(expected, actual)) {
        chrome.test.fail(error_msg +
                         "\nActual: " + JSON.stringify(actual) +
                         "\nExpected: " + JSON.stringify(expected));
      }
      return;
    }
    if (expected != actual) {
      chrome.test.fail(error_msg +
                       "\nActual: " + actual + "\nExpected: " + expected);
    }
    if (typeof(expected) != typeof(actual)) {
      chrome.test.fail(error_msg +
                       " (type mismatch)\nActual Type: " + typeof(actual) +
                       "\nExpected Type:" + typeof(expected));
    }
  };

  chrome.test.assertNoLastError = function() {
    if (chrome.runtime.lastError != undefined) {
      chrome.test.fail("lastError.message == " +
                       chrome.runtime.lastError.message);
    }
  };

  chrome.test.assertLastError = function(expectedError) {
    chrome.test.assertEq(typeof(expectedError), 'string');
    chrome.test.assertTrue(chrome.runtime.lastError != undefined,
        "No lastError, but expected " + expectedError);
    chrome.test.assertEq(expectedError, chrome.runtime.lastError.message);
  }

  function safeFunctionApply(func, args) {
    try {
      if (func)
        func.apply(null, args);
    } catch (e) {
      var msg = "uncaught exception " + e;
      chrome.test.fail(msg);
    }
  };

  // Wrapper for generating test functions, that takes care of calling
  // assertNoLastError() and (optionally) succeed() for you.
  chrome.test.callback = function(func, expectedError) {
    if (func) {
      chrome.test.assertEq(typeof(func), 'function');
    }
    var callbackCompleted = chrome.test.callbackAdded();

    return function() {
      if (expectedError == null) {
        chrome.test.assertNoLastError();
      } else {
        chrome.test.assertLastError(expectedError);
      }

      if (func) {
        safeFunctionApply(func, arguments);
      }

      callbackCompleted();
    };
  };

  chrome.test.listenOnce = function(event, func) {
    var callbackCompleted = chrome.test.callbackAdded();
    var listener = function() {
      event.removeListener(listener);
      safeFunctionApply(func, arguments);
      callbackCompleted();
    };
    event.addListener(listener);
  };

  chrome.test.listenForever = function(event, func) {
    var callbackCompleted = chrome.test.callbackAdded();

    var listener = function() {
      safeFunctionApply(func, arguments);
    };

    var done = function() {
      event.removeListener(listener);
      callbackCompleted();
    };

    event.addListener(listener);
    return done;
  };

  chrome.test.callbackPass = function(func) {
    return chrome.test.callback(func);
  };

  chrome.test.callbackFail = function(expectedError, func) {
    return chrome.test.callback(func, expectedError);
  };

  chrome.test.runTests = function(tests) {
    chrome.test.tests = tests;
    testCount = chrome.test.tests.length;
    chrome.test.runNextTest();
  };
