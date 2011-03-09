// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// extension_apitest.js
// mini-framework for ExtensionApiTest browser tests

var chrome = chrome || {};
(function() {
  chrome.test = chrome.test || {};

  chrome.test.tests = chrome.test.tests || [];

  var completed = false;
  var currentTest = null;
  var lastTest = null;

  function complete() {
    completed = true;

    // Try to get the script to stop running immediately.
    // This isn't an error, just an attempt at saying "done".
    throw "completed";
  }

  // Helper function to get around the fact that function names in javascript
  // are read-only, and you can't assign one to anonymous functions.
  function testName(test) {
    return test ? (test.name || test.generatedName) : "(no test)";
  }

  chrome.test.fail = function(message) {
    if (completed) throw "completed";
    chrome.test.log("(  FAILED  ) " + testName(currentTest));

    var stack = "(no stack available)";
    try {
      crash.me += 0;  // An intentional exception to get the stack trace.
    } catch (e) {
      if (typeof(e.stack) != undefined) {
        stack = e.stack.split("\n");
        stack = stack.slice(2);  // Remove title and fail() lines.
        stack = stack.join("\n");
      }
    }

    if (!message) {
      message = "FAIL (no message)";
    }
    message += "\n" + stack;
    console.log("[FAIL] " + testName(currentTest) + ": " + message);
    chrome.test.notifyFail(message);
    complete();
  };

  function allTestsSucceeded() {
    console.log("All tests succeeded");
    if (completed) throw "completed";

    chrome.test.notifyPass();
    complete();
  }

  var pendingCallbacks = 0;

  chrome.test.callbackAdded = function() {
    pendingCallbacks++;

    return function() {
      pendingCallbacks--;
      if (pendingCallbacks == 0) {
        chrome.test.succeed();
      }
    }
  };

  chrome.test.runNextTest = function() {
    chrome.test.assertEq(pendingCallbacks, 0);
    lastTest = currentTest;
    currentTest = chrome.test.tests.shift();
    if (!currentTest) {
      allTestsSucceeded();
      return;
    }
    try {
      chrome.test.log("( RUN      ) " + testName(currentTest));
      currentTest.call();
    } catch (e) {
      var message = e.stack || "(no stack available)";
      console.log("[FAIL] " + testName(currentTest) + ": " + message);
      chrome.test.notifyFail(message);
      complete();
    }
  };

  chrome.test.succeed = function() {
    console.log("[SUCCESS] " + testName(currentTest));
    chrome.test.log("(  SUCCESS )");
    // Use setTimeout here to allow previous test contexts to be
    // eligible for garbage collection.
    setTimeout(chrome.test.runNextTest, 0);
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
        if (message) {
          message = test + "\n" + message;
        } else {
          message = test;
        }
      }
      chrome.test.fail(message);
    }
  };

  chrome.test.checkDeepEq = function(expected, actual) {
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
    for (var p in actual) {
      if (typeof(expected[p]) == 'undefined')
        return false;
    }
    return true;
  };

  chrome.test.assertEq = function(expected, actual) {
    if (typeof(expected) == 'object') {
      if (!chrome.test.checkDeepEq(expected, actual)) {
        chrome.test.fail("API Test Error in " + testName(currentTest) +
                         "\nActual: " + JSON.stringify(actual) +
                         "\nExpected: " + JSON.stringify(expected));
      }
      return;
    }
    if (expected != actual) {
      chrome.test.fail("API Test Error in " + testName(currentTest) +
                       "\nActual: " + actual + "\nExpected: " + expected);
    }
    if (typeof(expected) != typeof(actual)) {
      chrome.test.fail("API Test Error in " + testName(currentTest) +
                       " (type mismatch)\nActual Type: " + typeof(actual) +
                       "\nExpected Type:" + typeof(expected));
    }
  };

  chrome.test.assertNoLastError = function() {
    if (chrome.extension.lastError != undefined) {
      chrome.test.fail("lastError.message == " +
                       chrome.extension.lastError.message);
    }
  };

  function safeFunctionApply(func, arguments) {
    try {
      if (func) {
        func.apply(null, arguments);
      }
    } catch (e) {
      var stack = "(no stack available)";
      if (typeof(e.stack) != "undefined") {
        stack = e.stack.toString();
      }
      var msg = "Exception during execution of callback in " +
                testName(currentTest);
      msg += "\n" + stack;
      console.log("[FAIL] " + testName(currentTest) + ": " + msg);
      chrome.test.notifyFail(msg);
      complete();
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
        chrome.test.assertEq(typeof(expectedError), 'string');
        chrome.test.assertTrue(chrome.extension.lastError != undefined,
            "No lastError, but expected " + expectedError);
        chrome.test.assertEq(expectedError, chrome.extension.lastError.message);
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
    chrome.test.runNextTest();
  };
})();
