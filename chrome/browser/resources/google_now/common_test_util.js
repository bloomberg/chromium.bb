// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common test utilities.

/**
 * Allows console.log output.
 */
var showConsoleLogOutput = false;

/**
 * Conditionally allow console.log output based off of showConsoleLogOutput.
 */
console.log = function() {
  var originalConsoleLog = console.log;
  return function() {
    if (showConsoleLogOutput) {
      originalConsoleLog.apply(console, arguments);
    }
  };
}();

function emptyMock() {}

// Container for event handlers added by mocked 'addListener' functions.
var mockEventHandlers = {};

/**
 * Mocks 'addListener' function of an API event. The mocked function will keep
 * track of handlers.
 * @param {Object} topLevelContainer Top-level container of the original
 *     function. Can be either 'chrome' or 'instrumented'.
 * @param {string} eventIdentifier Event identifier, such as
 *     'runtime.onSuspend'.
 */
function mockChromeEvent(topLevelContainer, eventIdentifier) {
  var eventIdentifierParts = eventIdentifier.split('.');
  var eventName = eventIdentifierParts.pop();
  var originalMethodContainer = topLevelContainer;
  var mockEventContainer = mockEventHandlers;
  eventIdentifierParts.forEach(function(fragment) {
    originalMethodContainer =
        originalMethodContainer[fragment] =
        originalMethodContainer[fragment] || {};
    mockEventContainer =
        mockEventContainer[fragment] =
        mockEventContainer[fragment] || {};
  });

  mockEventContainer[eventName] = [];
  originalMethodContainer[eventName] = {
    addListener: function(callback) {
      mockEventContainer[eventName].push(callback);
    }
  };
}

/**
 * Gets the array of event handlers added by a mocked 'addListener' function.
 * @param {string} eventIdentifier Event identifier, such as
 *     'runtime.onSuspend'.
 * @return {Array.<Function>} Array of handlers.
 */
function getMockHandlerContainer(eventIdentifier) {
  var eventIdentifierParts = eventIdentifier.split('.');
  var mockEventContainer = mockEventHandlers;
  eventIdentifierParts.forEach(function(fragment) {
    mockEventContainer = mockEventContainer[fragment];
  });

  return mockEventContainer;
}

/**
 * MockPromise
 * The JS test harness expects all calls to complete synchronously.
 * As a result, we can't use built-in JS promises since they run asynchronously.
 * Instead of mocking all possible calls to promises, a skeleton
 * implementation is provided to get the tests to pass.
 */
var Promise = function() {
  function PromisePrototypeObject(asyncTask) {
    var result;
    var resolved = false;
    asyncTask(
        function(asyncResult) {
          result = asyncResult;
          resolved = true;
        },
        function(asyncFailureResult) {
          result = asyncFailureResult;
          resolved = false;
        });

    function then(callback) {
      if (resolved) {
        callback.call(null, result);
      }
      return this;
    }

    // Promises use the function name "catch" to call back error handlers.
    // We can't use "catch" since function or variable names cannot use the word
    // "catch".
    function catchFunc(callback) {
      if (!resolved) {
        callback.call(null, result);
      }
      return this;
    }

    return {then: then, catch: catchFunc, isPromise: true};
  }

  function all(arrayOfPromises) {
    var results = [];
    for (i = 0; i < arrayOfPromises.length; i++) {
      if (arrayOfPromises[i].isPromise) {
        arrayOfPromises[i].then(function(result) {
          results[i] = result;
        });
      } else {
        results[i] = arrayOfPromises[i];
      }
    }
    var promise = new PromisePrototypeObject(function(resolve) {
      resolve(results);
    });
    return promise;
  }
  PromisePrototypeObject.all = all;
  return PromisePrototypeObject;
}();
