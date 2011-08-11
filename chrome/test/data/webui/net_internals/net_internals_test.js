// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The way these tests work is as follows:
 * C++ in net_internals_ui_browsertest.cc does any necessary setup, and then
 * calls the entry point for a test with RunJavascriptTest.  The called
 * function can then use the assert/expect functions defined in test_api.js.
 * All callbacks from the browser are wrapped in such a way that they can
 * also use the assert/expect functions.
 *
 * A test ends when an assert/expect test fails, an exception is thrown, or
 * |netInternalsTest.testDone| is called.  At that point, or soon afterwards,
 * the title is updated to 'Test Failed' if an assert/expect test fails, or
 * there was an exception.  Otherwise, it's set to 'Test Passed'.  The
 * behavior when an assert/expect test fails or an assertion is thrown only
 * after |netInternalsTest.testDone| is called is undefined.
 */

// Start of namespace.
var netInternalsTest = (function() {
  /**
   * A shorter poll interval is used for tests, since a few tests wait for
   * polled values to change.
   * @type {number}
   * @const
   */
  var TESTING_POLL_INTERVAL_MS = 50;

  /**
   * Indicates if the test is complete.
   * @type {boolean}
   */
  var done = false;

  /**
   * Updates the title of the page to report success or failure.  Must be
   * called at most once for each test.
   * @param {boolean} success Description of success param.
   */
  function updateTitle(success) {
    if (success) {
      document.title = 'Test Passed';
    } else {
      document.title = 'Test Failed';
    }
    done = true;
  }

  /**
   * Called to indicate a test is complete.
   */
  function testDone() {
    done = true;
  }

  /**
   * Creates a test function that can use the expect and assert functions
   * in test_api.js.  On failure, will set title to 'Test Failed', and when
   * a test is done and there was no failure, will set title to 'Test Passed'.
   * Calling expect/assert functions after done has been called has undefined
   * behavior.  Returned test functions can safely call each other directly.
   *
   * The resulting function has no return value.
   * @param {string} testName The name of the function, reported on error.
   * @param {Function} testFunction The function to run.
   * @return {function():void} Function that passes its parameters to
   *     testFunction, and passes the test result, if any, to the browser
   *     process by setting the window title.
   */
  function createTestFunction(testName, testFunction) {
    return function() {
      // Convert arguments to an array, as their map method may be called on
      // failure by runTestFunction.
      var testArguments = Array.prototype.slice.call(arguments, 0);

      // If the test is already complete, do nothing.
      if (done)
        return;

      var result = runTestFunction(testName, testFunction, testArguments);

      // If the first value is false, the test failed.
      if (!result[0]) {
        // Print any error messages.
        console.log(result[1]);
        // Update title to indicate failure.
        updateTitle(false);
      } else if (done)  {
        // If the first result is true, and |done| is also true, the test
        // passed.  Update title to indicate success.
        updateTitle(true);
      }
    };
  }

  /**
   * Dictionary of tests.
   * @type {Object.<string, Function>}
   */
  var tests = {};

  /**
   * Used to declare a test function called by the NetInternals browser test.
   * Takes in a name and a function, and adds it to the list of tests.
   * @param {string} testName The of the test.
   * @param {Function} testFunction The test function.
   */
  function test(testName, testFunction) {
    tests[testName] = testFunction;
  }

  /**
   * Called by the browser to start a test.  If constants haven't been
   * received from the browser yet, waits until they have been.
   * Experimentally, this never seems to happen, but may theoretically be
   * possible.
   * @param {string} testName The of the test to run.
   * @param {Function} testArguments The test arguments.
   */
  function runTest(testName, testArguments) {
    // If we've already received the constants, start the tests.
    if (typeof(LogEventType) != 'undefined') {
      startNetInternalsTest(testName, testArguments);
      return;
    }

    // Otherwise, wait until we do.
    console.log('Received constants late.');

    /**
     * Observer that starts the tests once we've received the constants.
     */
    function ConstantsObserver() {
      this.testStarted_ = false;
    }

    ConstantsObserver.prototype.onConstantsReceived = function() {
      if (!this.testStarted_) {
        this.testStarted_ = true;
        startNetInternalsTest(testFunction, testArguments);
      }
    };

    g_browser.addConstantsObserver(new ConstantsObserver());
  }

  /**
   * Starts running the test.  A test is run until an assert/expect statement
   * fails or testDone is called.  Those functions can only be called in the
   * test function body, or in response to a message dispatched by
   * |g_browser.receive|.
   * @param {string} testName The of the test to run.
   * @param {Function} testArguments The test arguments.
   */
  function startNetInternalsTest(testName, testArguments) {
    // Wrap g_browser.receive around a test function so that assert and expect
    // functions can be called from observers.
    g_browser.receive = createTestFunction('g_browser.receive', function() {
      BrowserBridge.prototype.receive.apply(g_browser, arguments);
    });

    g_browser.setPollInterval(TESTING_POLL_INTERVAL_MS);
    createTestFunction(testName, tests[testName]).apply(null, testArguments);
  }

  /**
   * Finds the first styled table that's a child of |parentId|, and returns the
   * number of rows it has.  Returns -1 if there's no such table.
   * @param {string} parentId HTML element id containing a styled table.
   * @return {number} Number of rows the style table's body has.
   */
  function getStyledTableNumRows(parentId) {
    // The tbody element of the first styled table in |parentId|.
    var tbody = document.querySelector('#' + parentId + ' .styledTable tbody');
    if (!tbody)
      return -1;
    return tbody.children.length;
  }

  /**
   * Finds the first styled table that's a child of the element with the given
   * id, and checks if it has exactly |expectedRows| rows, not including the
   * header row.
   * @param {string} parentId HTML element id containing a styled table.
   * @param {number} expectedRows Expected number of rows in the table.
   */
  function checkStyledTableRows(parentId, expectedRows) {
    expectEquals(expectedRows, getStyledTableNumRows(parentId),
                 'Incorrect number of rows in ' + parentId);
  }

  /**
   * Switches to the specified tab.
   * TODO(mmenke): check that the tab visibility changes as expected.
   * @param {string}: viewId Id of the view to switch to.
   */
  function switchToView(viewId) {
    document.location.hash = '#' + viewId;
  }

  // Exported functions.
  return {
    test: test,
    runTest: runTest,
    testDone:testDone,
    checkStyledTableRows: checkStyledTableRows,
    switchToView: switchToView
  };
})();

netInternalsTest.test('NetInternalsDone', function() {
  netInternalsTest.testDone();
});

netInternalsTest.test('NetInternalsExpectFail', function() {
  expectNotReached();
});

netInternalsTest.test('NetInternalsAssertFail', function() {
  assertNotReached();
});

netInternalsTest.test('NetInternalsObserverDone', function() {
  /**
   * A HostResolverInfo observer that calls testDone() in response to the
   * first seen event.
   */
  function HostResolverInfoObserver() {
  }

  HostResolverInfoObserver.prototype.onHostResolverInfoChanged = function() {
    netInternalsTest.testDone();
  };

  // Create the observer and add it to |g_browser|.
  g_browser.addHostResolverInfoObserver(new HostResolverInfoObserver());

  // Needed to trigger an update.
  netInternalsTest.switchToView('dns');
});

netInternalsTest.test('NetInternalsObserverExpectFail', function() {
  /**
   * A HostResolverInfo observer that triggers an exception in response to the
   * first seen event.
   */
  function HostResolverInfoObserver() {
  }

  HostResolverInfoObserver.prototype.onHostResolverInfoChanged = function() {
    expectNotReached();
    netInternalsTest.testDone();
  };

  // Create the observer and add it to |g_browser|.
  g_browser.addHostResolverInfoObserver(new HostResolverInfoObserver());

  // Needed to trigger an update.
  netInternalsTest.switchToView('dns');
});

netInternalsTest.test('NetInternalsObserverAssertFail', function() {
  /**
   * A HostResolverInfo observer that triggers an assertion in response to the
   * first seen event.
   */
  function HostResolverInfoObserver() {
  }

  HostResolverInfoObserver.prototype.onHostResolverInfoChanged = function() {
    assertNotReached();
  };

  // Create the observer and add it to |g_browser|.
  g_browser.addHostResolverInfoObserver(new HostResolverInfoObserver());

  // Needed to trigger an update.
  netInternalsTest.switchToView('dns');
});
