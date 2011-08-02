// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Library providing basic test framework functionality.
 **/

/**
 * Namespace for |Test|.
 * @type {Object}
 **/
var testing = {};

/**
 * Hold the currentTestCase across between PreLoad and Run.
 * @type {TestCase}
 **/
var currentTestCase = null;

(function() {
  // Provide global objects for generation case.
  if (this['window'] === undefined)
    this['window'] = this;
  if (this['chrome'] === undefined) {
    this['chrome'] = {
      send: function() {},
    };
  }
  if (this['console'] === undefined) {
    this['console'] = {
      log: print,
    };
  }

  /**
   * This class will be exported as testing.Test, and is provided to hold the
   * fixture's configuration and callback methods for the various phases of
   * invoking a test. It is called "Test" rather than TestFixture to roughly
   * mimic the gtest's class names.
   * @constructor
   **/
  function Test() {}

  Test.prototype = {
    /**
     * The name of the test.
     **/
    name: null,

    /**
     * When set to a string value representing a url, generate BrowsePreload
     * call, which will browse to the url and call fixture.PreLoad of the
     * currentTestCase.
     * @type {string}
     **/
    browsePreload: null,

    /**
     * When set to a string value representing an html page in the test
     * directory, generate BrowsePrintPreload call, which will browse to a url
     * representing the file, cause print, and call fixture.PreLoad of the
     * currentTestCase.
     * @type {string}
     **/
    browsePrintPreload: null,

    /**
     * When set to a function, will be called in the context of the test
     * generation inside the function, and before any generated C++.
     * @type {function(string,string)}
     **/
    testGenPreamble: null,

    /**
     * When set to a function, will be called in the context of the test
     * generation inside the function, and after any generated C++.
     * @type {function(string,string)}
     **/
    testGenPostamble: null,

    /**
     * When set to a non-null string, auto-generate typedef before generating
     * TEST*: {@code typedef typedefCppFixture testFixture}.
     * @type {string}
     **/
    typedefCppFixture: 'WebUIBrowserTest',

    /**
     * This should be initialized by the test fixture and can be referenced
     * during the test run.
     * @type {Mock4JS.Mock}
     **/
    mockHandler: null,

    /**
     * Override this method to perform initialization during preload (such as
     * creating mocks and registering handlers).
     * @type {Function}
     **/
    PreLoad: function() {},

    /**
     * Override this method to perform tasks before running your test.
     * @type {Function}
     **/
    SetUp: function() {},

    /**
     * Override this method to perform tasks after running your test. If you
     * create a mock class, you must call Mock4JS.verifyAllMocks() in this
     * phase.
     * @type {Function}
     **/
    TearDown: function() {
      Mock4JS.verifyAllMocks();
    }
  };

  /**
   * This class is not exported and is available to hold the state of the
   * |currentTestCase| throughout preload and test run.
   * @param {string} name The name of the test case.
   * @param {Test} fixture The fixture object for this test case.
   * @param {Function} body The code to run for the test.
   * @constructor
   **/
  function TestCase(name, fixture, body) {
    this.name = name;
    this.fixture = fixture;
    this.body = body;
  }

  TestCase.prototype = {
    name: null,
    fixture: null,
    body: null,

    /**
     * Called at preload time, proxies to the fixture.
     * @type {Function}
     **/
    PreLoad: function(name) {
      if (this.fixture)
        this.fixture.PreLoad();
    },

    /**
     * Runs this test case with |this| set to the |fixture|.
     *
     * Note: Tests created with TEST_F may depend upon |this| being set to an
     * instance of this.fixture. The current implementation of TEST creates a
     * dummy constructor, but tests created with TEST should not rely on |this|
     * being set.
     * @type {Function}
     **/
    Run: function() {
      if (this.fixture)
        this.fixture.SetUp();
      if (this.body)
        this.body.call(this.fixture);
      if (this.fixture)
        this.fixture.TearDown();
    },
  };

  /**
   * Registry of javascript-defined callbacks for {@code chrome.send}.
   * @type {Object}
   **/
  var sendCallbacks = {};

  /**
   * Registers the message, object and callback for {@code chrome.send}
   * @param {string} name The name of the message to route to this |callback|.
   * @param {Object} messageHAndler Pass as |this| when calling the |callback|.
   * @param {function(...)} callback Called by {@code chrome.send}.
   * @see sendCallbacks
   **/
  function registerMessageCallback(name, messageHandler, callback) {
    sendCallbacks[name] = [messageHandler, callback];
  }

  /**
   * Register all methods of {@code mockClass.prototype} with messages of the
   * same name as the method, using the proxy of the |mockObject| as the
   * |messageHandler| when registering.
   * @param {Mock4JS.Mock} mockObject The mock to register callbacks against.
   * @param {function(new:Object)} mockClAss Constructor for the mocked class.
   * @see registerMessageCallback
   **/
  function registerMockMessageCallbacks(mockObject, mockClass) {
    var mockProxy = mockObject.proxy();
    for (func in mockClass.prototype) {
      if (typeof(mockClass.prototype[func]) == 'function') {
        registerMessageCallback(func,
                                mockProxy,
                                mockProxy[func]);
      }
    }
  }

  /**
   * Overrides {@code chrome.send} for routing messages to javascript
   * functions. Also fallsback to sending with the original chrome object.
   * @param {string} messageName The message to route.
   **/
  function send(messageName) {
    var callback = sendCallbacks[messageName];
    var args = Array.prototype.slice.call(arguments, 1);
    if (callback != undefined)
      callback[1].apply(callback[0], args);
    else
      this.__proto__.send.apply(this.__proto__, args);
  }

  /**
   * Provides a mechanism for assert* and expect* methods to fetch the signature
   * of their caller. Assert* methods should |registerCall| and expect* methods
   * should set |isExpect| and |expectName| properties to indicate that the
   * interesting caller is one more level up the stack.
   **/
  function CallHelper() {
    this.__proto__ = CallHelper.prototype;
  }

  CallHelper.prototype = {
    /**
     * Holds the mapping of (callerCallerString, callerName) -> count of times
     * called.
     * @type {Object.<string, Object.<string, number>>}
     **/
    counts_: {},

    /**
     * This information about the caller is needed from most of the following
     * routines.
     * @param {Function} caller the caller of the assert* routine.
     * @return {{callerName: string, callercallerString: string}} stackInfo
     * @private
     **/
    getCallerInfo_: function(caller) {
      var callerName = caller.name;
      var callerCaller = caller.caller;
      if (callerCaller['isExpect']) {
        callerName = callerCaller.expectName;
        callerCaller = callerCaller.caller;
      }
      var callerCallerString = callerCaller.toString();
      return {
        callerName: callerName,
        callerCallerString: callerCallerString,
      };
    },

    /**
     * Register a call to an assertion class.
     **/
    registerCall: function() {
      var stackInfo = this.getCallerInfo_(arguments.callee.caller);
      if (!(stackInfo.callerCallerString in this.counts_))
        this.counts_[stackInfo.callerCallerString] = {};
      if (!(stackInfo.callerName in this.counts_[stackInfo.callerCallerString]))
        this.counts_[stackInfo.callerCallerString][stackInfo.callerName] = 0;
      ++this.counts_[stackInfo.callerCallerString][stackInfo.callerName];
    },

    /**
     * Get the call signature of this instance of the caller's call to this
     * function.
     * @param {Function} caller The caller of the assert* routine.
     * @return {String} Call signature.
     * @private
     **/
    getCall_: function(caller) {
      var stackInfo = this.getCallerInfo_(caller);
      var count =
          this.counts_[stackInfo.callerCallerString][stackInfo.callerName];

      // Allow pattern to match multiple lines for text wrapping.
      var callerRegExp =
          new RegExp(stackInfo.callerName + '\\((.|\\n)*?\\);', 'g');

      // Find all matches allowing wrap around such as when a helper function
      // calls assert/expect calls and that helper function is called multiple
      // times.
      var matches = stackInfo.callerCallerString.match(callerRegExp);
      var match = matches[(count - 1) % matches.length];

      // Chop off the trailing ';'.
      return match.substring(0, match.length-1);
    },

    /**
     * Returns the text of the call signature and any |message|.
     * @param {string=} message Addtional message text from caller.
     **/
    getCallMessage: function(message) {
      var callMessage = this.getCall_(arguments.callee.caller);
      if (message)
        callMessage += ': ' + message;
      return callMessage;
    },
  };

  /**
   * Help register calls for better error reporting.
   * @type {CallHelper}
   **/
  var helper = new CallHelper();

  // Asserts.
  // Use the following assertions to verify a condition within a test.
  // If assertion fails, the C++ backend will be immediately notified.
  // If assertion passes, no notification will be sent to the C++ backend.

  /**
   * When |test| !== true, aborts the current test.
   * @param {boolean} test The predicate to check against |expected|.
   * @param {string=} message The message to include in the Error thrown.
   * @throws {Error} upon failure.
   **/
  function assertTrue(test, message) {
    helper.registerCall();
    if (test !== true)
      throw new Error(
          'Test Error ' + helper.getCallMessage(message) + ': ' + test);
  }

  /**
   * When |test| !== false, aborts the current test.
   * @param {boolean} test The predicate to check against |expected|.
   * @param {string=} message The message to include in the Error thrown.
   * @throws {Error} upon failure.
   **/
  function assertFalse(test, message) {
    helper.registerCall();
    if (test !== false)
      throw new Error(
          'Test Error ' + helper.getCallMessage(message) + ': ' + test);
  }

  /**
   * When |val1| < |val2|, aborts the current test.
   * @param {number} val1 The number expected to be >= |val2|.
   * @param {number} val2 The number expected to be < |val1|.
   * @param {string=} message The message to include in the Error thrown.
   **/
  function assertGE(val1, val2, message) {
    helper.registerCall();
    if (val1 < val2) {
      throw new Error(
          'Test Error ' + helper.getCallMessage(message) + val1 + '<' + val2);
    }
  }

  /**
   * When |val1| <= |val2|, aborts the current test.
   * @param {number} val1 The number expected to be > |val2|.
   * @param {number} val2 The number expected to be <= |val1|.
   * @param {string=} message The message to include in the Error thrown.
   **/
  function assertGT(val1, val2, message) {
    helper.registerCall();
    if (val1 <= val2) {
      throw new Error(
          'Test Error ' + helper.getCallMessage(message) + val1 + '<=' + val2);
    }
  }

  /**
   * When |expected| !== |actual|, aborts the current test.
   * @param {*} expected The expected value of |actual|.
   * @param {*} actual The predicate to check against |expected|.
   * @param {string=} message The message to include in the Error thrown.
   * @throws {Error} upon failure.
   **/
  function assertEquals(expected, actual, message) {
    helper.registerCall();
    if (expected != actual) {
      throw new Error(
          'Test Error ' + helper.getCallMessage(message) +
          '\nActual: ' + actual + '\nExpected: ' + expected);
    }
    if (typeof expected != typeof actual) {
      throw new Error(
          'Test Error (type mismatch) ' + helper.getCallMessage(message) +
          '\nActual Type: ' + typeof actual +
          '\nExpected Type:' + typeof expected);
    }
  }

  /**
   * When |val1| > |val2|, aborts the current test.
   * @param {number} val1 The number expected to be <= |val2|.
   * @param {number} val2 The number expected to be > |val1|.
   * @param {string=} message The message to include in the Error thrown.
   **/
  function assertLE(val1, val2, message) {
    helper.registerCall();
    if (val1 > val2) {
      throw new Error(
          'Test Error ' + helper.getCallMessage(message) + val1 + '>' + val2);
    }
  }

  /**
   * When |val1| >= |val2|, aborts the current test.
   * @param {number} val1 The number expected to be < |val2|.
   * @param {number} val2 The number expected to be >= |val1|.
   * @param {string=} message The message to include in the Error thrown.
   **/
  function assertLT(val1, val2, message) {
    helper.registerCall();
    if (val1 >= val2) {
      throw new Error(
          'Test Error ' + helper.getCallMessage(message) + val1 + '>=' + val2);
    }
  }

  /**
   * When |notExpected| === |actual|, aborts the current test.
   * @param {*} notExpected The expected value of |actual|.
   * @param {*} actual The predicate to check against |notExpected|.
   * @param {string=} message The message to include in the Error thrown.
   * @throws {Error} upon failure.
   **/
  function assertNotEquals(notExpected, actual, message) {
    helper.registerCall();
    if (notExpected === actual) {
      throw new Error(
          'Test Error ' + helper.getCallMessage(message) +
          '\nActual: ' + actual + '\nnotExpected: ' + notExpected);
    }
  }

  /**
   * Always aborts the current test.
   * @param {string=} message The message to include in the Error thrown.
   * @throws {Error} always.
   **/
  function assertNotReached(message) {
    helper.registerCall();
    throw new Error(helper.getCallMessage(message));
  }

  /**
   * Holds the errors, if any, caught by expects so that the test case can fail.
   * @type {Array.<Error>}
   **/
  var errors = [];

  /**
   * Creates a function based upon a function that thows an exception on
   * failure. The new function stuffs any errors into the |errors| array for
   * checking by runTest. This allows tests to continue running other checks,
   * while failing the overal test if any errors occurrred.
   * @param {Function} assertFunc The function which may throw an Error.
   * @return {Function} A function that applies its arguments to |assertFunc|.
   * @see errors
   * @see runTest
   **/
  function createExpect(assertFunc) {
    var expectFunc = function() {
      try {
        assertFunc.apply(null, arguments);
      } catch (e) {
        errors.push(e);
      }
    };
    expectFunc.isExpect = true;
    expectFunc.expectName = assertFunc.name.replace(/^assert/, 'expect');
    return expectFunc;
  }

  /**
   * This is the starting point for tests run by WebUIBrowserTest. It clears
   * |errors|, runs the test surrounded by an expect to catch Errors. If
   * |errors| is non-empty, it reports a failure and a message by joining
   * |errors|.
   * @param {string} testFunction The function name to call.
   * @param {Array} testArguments The arguments to call |testFunction| with.
   * @return {Array.<boolean, string>} [test-succeeded, message-if-failed]
   * @see errors
   * @see createExpect
   **/
  function runTest(testFunction, testArguments) {
    errors.splice(0, errors.length);
    // Avoid eval() if at all possible, since it will not work on pages
    // that have enabled content-security-policy.
    var testBody = this[testFunction];    // global object -- not a method.
    if (typeof testBody === "undefined")
      testBody = eval(testFunction);
    if (testBody != RUN_TEST_F) {
      var testName =
          testFunction.name ? testFunction.name : testBody.toString();
      console.log('Running test ' + testName);
    }
    createExpect(testBody).apply(null, testArguments);

    var result = [true];
    if (errors.length) {
      var message = '';
      for (var i = 0; i < errors.length; ++i) {
        message += 'Failed: ' + testFunction + '(' +
                   testArguments.map(JSON.stringify) +
                   ')\n' + errors[i].stack;
      }
      errors.splice(0, errors.length);
      result = [false, message];
    }
    return result;
  }

  /**
   * Creates a new test case for the given |testFixture| and |testName|. Assumes
   * |testFixture| describes a globally available subclass of type Test.
   * @param {string} testFixture The fixture for this test case.
   * @param {string} testName The name for this test case.
   * @return {TestCase} A newly created TestCase.
   **/
  function createTestCase(testFixture, testName) {
    var fixtureConstructor = this[testFixture];
    var testBody = fixtureConstructor.testCaseBodies[testName];
    var fixture = new fixtureConstructor();
    fixture.name = testFixture;
    return new TestCase(testName, fixture, testBody);
  }

  /**
   * Used by WebUIBrowserTest to preload the javascript libraries at the
   * appropriate time for javascript injection into the current page. This
   * creates a test case and calls its PreLoad for any early initialization such
   * as registering handlers before the page's javascript runs it's OnLoad
   * method. This is called before the page is loaded, so the |chrome| object is
   * not yet bound and this DOMContentLoaded listener will be called first to
   * override |chrome| in order to route messages registered in |sendCallbacks|.
   * @param {string} testFixture The test fixture name.
   * @param {string} testName The test name.
   * @see sendCallbacks
   **/
  function preloadJavascriptLibraries(testFixture, testName) {
    window.addEventListener('DOMContentLoaded', function() {
      var oldChrome = chrome;
      chrome = {
        __proto__: oldChrome,
        send: send,
      };
    });
    currentTestCase = createTestCase(testFixture, testName);
    currentTestCase.PreLoad();
  }

  /**
   * During generation phase, this outputs; do nothing at runtime.
   **/
  function GEN() {}

  /**
   * At runtime, register the testName with a test fixture. Since this method
   * doesn't have a test fixture, create a dummy fixture to hold its |name|
   * and |testCaseBodies|.
   * @param {string} testCaseName The name of the test case.
   * @param {string} testName The name of the test function.
   * @param {Function} testBody The body to execute when running this test.
   **/
  function TEST(testCaseName, testName, testBody) {
    var fixtureConstructor = this[testCaseName];
    if (fixtureConstructor === undefined) {
      fixtureConstructor = function() {};
      this[testCaseName] = fixtureConstructor;
      fixtureConstructor.prototype = {
        __proto__: Test.prototype,
        name: testCaseName,
      };
      fixtureConstructor.testCaseBodies = {};
    }
    fixtureConstructor.testCaseBodies[testName] = testBody;
  }

  /**
   * At runtime, register the testName with its fixture. Stuff the |name| into
   * the |testFixture|'s prototype, if needed, and the |testCaseBodies| into its
   * constructor.
   * @param {string} testFixture The name of the test fixture class.
   * @param {string} testName The name of the test function.
   * @param {Function} testBody The body to execute when running this test.
   **/
  function TEST_F(testFixture, testName, testBody) {
    var fixtureConstructor = this[testFixture];
    if (!fixtureConstructor.prototype.name)
      fixtureConstructor.prototype.name = testFixture;
    if (fixtureConstructor['testCaseBodies'] === undefined)
      fixtureConstructor.testCaseBodies = {};
    fixtureConstructor.testCaseBodies[testName] = testBody;
  }

  /**
   * RunJavascriptTestF uses this as the |testFunction| when invoking
   * runTest. If |currentTestCase| is non-null at this point, verify that
   * |testFixture| and |testName| agree with the preloaded values. Create
   * |currentTestCase|, if needed, run it, and clear the |currentTestCase|.
   * @param {string} testFixture The name of the test fixture class.
   * @param {string} testName The name of the test function.
   * @see preloadJavascriptLibraries
   * @see runTest
   **/
  function RUN_TEST_F(testFixture, testName) {
    try {
      if (!currentTestCase)
        currentTestCase = createTestCase(testFixture, testName);
      assertEquals(currentTestCase.name, testName);
      assertEquals(currentTestCase.fixture.name, testFixture);
      console.log('Running TestCase ' + testFixture + '.' + testName);
      currentTestCase.Run();
    } finally {
      currentTestCase = null;
    }
  }

  /**
   * CallFunctionAction is provided to allow mocks to have side effects.
   * @param {Function} func The function to call.
   * @param {Array} args Any arguments to pass to func.
   * @constructor
   **/
  function CallFunctionAction(func, args) {
    this._func = func;
    this._args = args;
  }

  CallFunctionAction.prototype = {
    invoke: function() {
      return this._func.apply(null, this._args);
    },
    describe: function() {
      return 'calls the given function with arguments ' + this._args;
    }
  };

  /**
   * Syntactic sugar for will() on a Mock4JS.Mock.
   * @param {Function} func the function to call when the method is invoked.
   * @param {...*} var_args arguments to pass when calling func.
   **/
  function callFunction(func) {
    return new CallFunctionAction(func,
                                  Array.prototype.slice.call(arguments, 1));
  }

  /**
   * Allow mock stubs() and expects() to know what arguments were passed to the
   * |realMatcher|.
   * @param {!Object} realMatcher The real matcher to perform matching with.
   * @constructor
   **/
  function SaveArgumentsMatcher(realMatcher) {
    this.realMatcher_ = realMatcher;
  }

  SaveArgumentsMatcher.prototype = {
    /**
     * Remember the argument passed to this stub invocation.
     * @type {*}
     **/
    argument: undefined,

    /**
     * @type {Object} the object performing the real match.
     * @private
     */
    realMatcher_: null,

    /**
     * Saves |actualArgument| for later use by the mock stub or expect.
     * @param {*} actualArgument The argument to match and save.
     * @return {boolean} value of calling the |realMatcher_|.
     **/
    argumentMatches: function(actualArgument) {
      this.argument = actualArgument;
      return this.realMatcher_.argumentMatches.call(this.realMatcher_,
                                                    actualArgument);
    },

    /**
     * Generic description for this Matcher object.
     * @return {string} description of the matcher for this argument.
     **/
    describe: function() {
      return 'SaveArguments(' +
          this.realMatcher_.describe.call(this.realMatcher_) + ')';
    },
  };

  // Exports.
  testing.Test = Test;
  window.assertTrue = assertTrue;
  window.assertFalse = assertFalse;
  window.assertGE = assertGE;
  window.assertGT = assertGT;
  window.assertEquals = assertEquals;
  window.assertLE = assertLE;
  window.assertLT = assertLT;
  window.assertNotEquals = assertNotEquals;
  window.assertNotReached = assertNotReached;
  window.callFunction = callFunction;
  window.expectTrue = createExpect(assertTrue);
  window.expectFalse = createExpect(assertFalse);
  window.expectGE = createExpect(assertGE);
  window.expectGT = createExpect(assertGT);
  window.expectEquals = createExpect(assertEquals);
  window.expectLE = createExpect(assertLE);
  window.expectLT = createExpect(assertLT);
  window.expectNotEquals = createExpect(assertNotEquals);
  window.expectNotReached = createExpect(assertNotReached);
  window.preloadJavascriptLibraries = preloadJavascriptLibraries;
  window.registerMessageCallback = registerMessageCallback;
  window.registerMockMessageCallbacks = registerMockMessageCallbacks;
  window.runTest = runTest;
  window.SaveArgumentsMatcher = SaveArgumentsMatcher;
  window.TEST = TEST;
  window.TEST_F = TEST_F;
  window.GEN = GEN;

  // Import the Mock4JS helpers.
  Mock4JS.addMockSupport(window);
})();
