// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library providing basic test framework functionality.

(function() {
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
      throw new Error(message);
    }
  }

  var old_chrome = chrome;
  var send_callbacks = {};

  function registerMessageCallback(name, object, callback) {
    send_callbacks[name] = [object, callback];
  }

  function send(messageName) {
    var callback = send_callbacks[messageName];
    var args = Array.prototype.slice.call(arguments, 1);
    if (callback != undefined)
      callback[1].apply(callback[0], args);
    else
      old_chrome.send.apply(old_chrome, args);
  }

  function assertTrue(test, message) {
    assertBool(test, true, message);
  }

  function assertFalse(test, message) {
    assertBool(test, false, message);
  }

  function assertEquals(expected, actual, message) {
    if (expected != actual) {
      throw new Error('Test Error. Actual: ' + actual + '\nExpected: ' +
                       expected + '\n' + message);
    }
    if (typeof expected != typeof actual) {
      throw new Error('Test Error' +
                      ' (type mismatch)\nActual Type: ' + typeof actual +
                      '\nExpected Type:' + typeof expected + '\n' + message);
    }
  }

  function assertNotReached(message) {
    throw new Error(message);
  }

  var errors = [];

  function createExpect(assertFunc) {
    return function() {
      try {
        assertFunc.apply(null, arguments);
      } catch (e) {
        console.log('Failed: ' + currentTest.name + '\n' + e.stack);
        errors.push(e);
      }
    };
  }

  function runTest(testFunction, testArguments) {
    errors = [];
    // Avoid eval() if at all possible, since it will not work on pages
    // that have enabled content-security-policy.
    currentTest = this[testFunction];    // global object -- not a method.
    if (typeof currentTest === "undefined") {
      currentTest = eval(testFunction);
    }
    console.log('Running test ' + currentTest.name);
    createExpect(currentTest).apply(null, testArguments);

    if (errors.length) {
      return [false, errors.join('\n')];
    }

    return [true];
  }

  function preloadJavascriptLibraries(overload_chrome_send) {
    if (overload_chrome_send)
      chrome = { 'send': send };
  }

  // Exports.
  window.assertTrue = assertTrue;
  window.assertFalse = assertFalse;
  window.assertEquals = assertEquals;
  window.assertNotReached = assertNotReached;
  window.expectTrue = createExpect(assertTrue);
  window.expectFalse = createExpect(assertFalse);
  window.expectEquals = createExpect(assertEquals);
  window.expectNotReached = createExpect(assertNotReached);
  window.registerMessageCallback = registerMessageCallback;
  window.runTest = runTest;
  window.preloadJavascriptLibraries = preloadJavascriptLibraries;
})();
