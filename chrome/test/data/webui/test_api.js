// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Library providing basic test framework functionality.
 */

/**
 * Namespace for |Test|.
 * @type {Object}
 */
var testing = {};
(function(exports) {
  /**
   * Holds the original version of the |chrome| object.
   */
  var originalChrome = null;

  /**
   * Hold the currentTestCase across between preLoad and run.
   * @type {TestCase}
   */
  var currentTestCase = null;

  /**
   * The string representation of the currently running test function.
   * @type {?string}
   */
  var currentTestFunction = null;

  /**
   * The arguments of the currently running test.
   * @type {Array}
   */
  var currentTestArguments = [];

 /**
   * This class will be exported as testing.Test, and is provided to hold the
   * fixture's configuration and callback methods for the various phases of
   * invoking a test. It is called "Test" rather than TestFixture to roughly
   * mimic the gtest's class names.
   * @constructor
   */
  function Test() {};

  Test.prototype = {
    /**
     * The name of the test.
     */
    name: null,

    /**
     * When set to a string value representing a url, generate BrowsePreload
     * call, which will browse to the url and call fixture.preLoad of the
     * currentTestCase.
     * @type {string}
     */
    browsePreload: null,

    /**
     * When set to a string value representing an html page in the test
     * directory, generate BrowsePrintPreload call, which will browse to a url
     * representing the file, cause print, and call fixture.preLoad of the
     * currentTestCase.
     * @type {string}
     */
    browsePrintPreload: null,

    /**
     * When set to a function, will be called in the context of the test
     * generation inside the function, after AddLibrary calls and before
     * generated C++.
     * @type {function(string,string)}
     */
    testGenPreamble: null,

    /**
     * When set to a function, will be called in the context of the test
     * generation inside the function, and after any generated C++.
     * @type {function(string,string)}
     */
    testGenPostamble: null,

    /**
     * When set to a non-null string, auto-generate typedef before generating
     * TEST*: {@code typedef typedefCppFixture testFixture}.
     * @type {string}
     */
    typedefCppFixture: 'WebUIBrowserTest',

    /**
     * This should be initialized by the test fixture and can be referenced
     * during the test run. It holds any mocked handler methods.
     * @type {?Mock4JS.Mock}
     */
    mockHandler: null,

    /**
     * This should be initialized by the test fixture and can be referenced
     * during the test run. It holds any mocked global functions.
     * @type {?Mock4JS.Mock}
     */
    mockGlobals: null,

    /**
     * Value is passed through call to C++ RunJavascriptF to invoke this test.
     * @type {boolean}
     */
    isAsync: false,

    /**
     * True when the test is expected to fail for testing the test framework.
     * @type {boolean}
     */
    testShouldFail: false,

    /**
     * Extra libraries to add before loading this test file.
     * @type {Array.<string>}
     */
    extraLibraries: [],

    /**
     * Extra libraries to add before loading this test file.
     * This list is in the form of Closure library style object
     * names.  To support this, a closure deps.js file must
     * be specified when generating the test C++ source.
     * The specified libraries will be included with their transitive
     * dependencies according to the deps file.
     * @type {Array.<string>}
     */
    closureModuleDeps: [],

    /**
     * Whether to run the accessibility checks.
     * @type {boolean}
     */
    runAccessibilityChecks: true,

    /**
     * Configuration for the accessibility audit.
     * @type {axs.AuditConfiguration}
     */
    accessibilityAuditConfig_: null,

    /**
     * Returns the configuration for the accessibility audit, creating it
     * on-demand.
     * @return {axs.AuditConfiguration}
     */
    get accessibilityAuditConfig() {
      if (!this.accessibilityAuditConfig_) {
        this.accessibilityAuditConfig_ = new axs.AuditConfiguration();

        this.accessibilityAuditConfig_.showUnsupportedRulesWarning = false;

        this.accessibilityAuditConfig_.auditRulesToIgnore = [
            // The "elements with meaningful background image" accessibility
            // audit (AX_IMAGE_01) does not apply, since Chrome doesn't
            // disable background images in high-contrast mode like some
            // browsers do.
            "elementsWithMeaningfulBackgroundImage",

            // Most WebUI pages are inside an IFrame, so the "web page should
            // have a title that describes topic or purpose" test (AX_TITLE_01)
            // generally does not apply.
            "pageWithoutTitle",

            // TODO(aboxhall): re-enable when crbug.com/267035 is fixed.
            // Until then it's just noise.
            "lowContrastElements",
        ];
      }
      return this.accessibilityAuditConfig_;
    },

    /**
     * Whether to treat accessibility issues (errors or warnings) as test
     * failures. If true, any accessibility issues will cause the test to fail.
     * If false, accessibility issues will cause a console.warn.
     * Off by default to begin with; as we add the ability to suppress false
     * positives, we will transition this to true.
     * @type {boolean}
     */
    accessibilityIssuesAreErrors: false,

    /**
     * Holds any accessibility results found during the accessibility audit.
     * @type {Array.<Object>}
     */
    a11yResults_: [],

    /**
     * Gets the list of accessibility errors found during the accessibility
     * audit. Only for use in testing.
     * @return {Array.<Object>}
     */
    getAccessibilityResults: function() {
      return this.a11yResults_;
    },

    /**
     * Run accessibility checks after this test completes.
     */
    enableAccessibilityChecks: function() {
      this.runAccessibilityChecks = true;
    },

    /**
     * Don't run accessibility checks after this test completes.
     */
    disableAccessibilityChecks: function() {
      this.runAccessibilityChecks = false;
    },

    /**
     * Create a new class to handle |messageNames|, assign it to
     * |this.mockHandler|, register its messages and return it.
     * @return {Mock} Mock handler class assigned to |this.mockHandler|.
     */
    makeAndRegisterMockHandler: function(messageNames) {
      var MockClass = makeMockClass(messageNames);
      this.mockHandler = mock(MockClass);
      registerMockMessageCallbacks(this.mockHandler, MockClass);
      return this.mockHandler;
    },

    /**
     * Create a new class to handle |functionNames|, assign it to
     * |this.mockGlobals|, register its global overrides, and return it.
     * @return {Mock} Mock handler class assigned to |this.mockGlobals|.
     * @see registerMockGlobals
     */
    makeAndRegisterMockGlobals: function(functionNames) {
      var MockClass = makeMockClass(functionNames);
      this.mockGlobals = mock(MockClass);
      registerMockGlobals(this.mockGlobals, MockClass);
      return this.mockGlobals;
    },

    /**
      * Create a container of mocked standalone functions to handle
      * '.'-separated |apiNames|, assign it to |this.mockApis|, register its API
      * overrides and return it.
      * @return {Mock} Mock handler class.
      * @see makeMockFunctions
      * @see registerMockApis
      */
    makeAndRegisterMockApis: function (apiNames) {
      var apiMockNames = apiNames.map(function(name) {
        return name.replace(/\./g, '_');
      });

      this.mockApis = makeMockFunctions(apiMockNames);
      registerMockApis(this.mockApis);
      return this.mockApis;
    },

    /**
      * Create a container of mocked standalone functions to handle
      * |functionNames|, assign it to |this.mockLocalFunctions| and return it.
      * @param {!Array.<string>} functionNames
      * @return {Mock} Mock handler class.
      * @see makeMockFunctions
      */
    makeMockLocalFunctions: function(functionNames) {
      this.mockLocalFunctions = makeMockFunctions(functionNames);
      return this.mockLocalFunctions;
    },

    /**
     * Override this method to perform initialization during preload (such as
     * creating mocks and registering handlers).
     * @type {Function}
     */
    preLoad: function() {},

    /**
     * Override this method to perform tasks before running your test.
     * @type {Function}
     */
    setUp: function() {},

    /**
     * Override this method to perform tasks after running your test. If you
     * create a mock class, you must call Mock4JS.verifyAllMocks() in this
     * phase.
     * @type {Function}
     */
    tearDown: function() {
      Mock4JS.verifyAllMocks();
    },

    /**
     * Called to run the body from the perspective of this fixture.
     * @type {Function}
     */
    runTest: function(testBody) {
      testBody.call(this);
    },

    /**
     * Called to run the accessibility audit from the perspective of this
     * fixture.
     */
    runAccessibilityAudit: function() {
      if (!this.runAccessibilityChecks || typeof document === 'undefined')
        return;

      var auditConfig = this.accessibilityAuditConfig;
      if (!runAccessibilityAudit(this.a11yResults_, auditConfig)) {
        var report = accessibilityAuditReport(this.a11yResults_);
        if (this.accessibilityIssuesAreErrors)
          throw new Error(report);
        else
          console.warn(report);
      }
    },

    /**
     * Create a closure function for continuing the test at a later time. May be
     * used as a listener function.
     * @param {WhenTestDone} whenTestDone Call testDone() at the appropriate
     *     time.
     * @param {Function} completion The function to call to complete the test.
     * @param {...*} var_args Arguments to pass when calling completionAction.
     * @return {function(): void} Return a function, bound to this test fixture,
     *     which continues the test.
     */
    continueTest: function(whenTestDone, completion) {
      var savedArgs = new SaveMockArguments();
      var completionAction = new CallFunctionAction(
          this, savedArgs, completion,
          Array.prototype.slice.call(arguments, 2));
      if (whenTestDone === WhenTestDone.DEFAULT)
        whenTestDone = WhenTestDone.ASSERT;
      var runAll = new RunAllAction(
          true, whenTestDone, [completionAction]);
      return function() {
        savedArgs.arguments = Array.prototype.slice.call(arguments);
        runAll.invoke();
      };
    },

    /**
     * Call this during setUp to defer the call to runTest() until later. The
     * caller must call the returned function at some point to run the test.
     * @type {Function}
     * @param {WhenTestDone} whenTestDone Call testDone() at the appropriate
     *     time.
     * @param {...*} var_args Arguments to pass when running the
     *     |currentTestCase|.
     * @return {function(): void} A function which will run the current body of
     *     the currentTestCase.
     */
    deferRunTest: function(whenTestDone) {
      if (whenTestDone === WhenTestDone.DEFAULT)
        whenTestDone = WhenTestDone.ALWAYS;

      return currentTestCase.deferRunTest.apply(
          currentTestCase, [whenTestDone].concat(
              Array.prototype.slice.call(arguments, 1)));
    },
  };

  /**
   * This class is not exported and is available to hold the state of the
   * |currentTestCase| throughout preload and test run.
   * @param {string} name The name of the test case.
   * @param {Test} fixture The fixture object for this test case.
   * @param {Function} body The code to run for the test.
   * @constructor
   */
  function TestCase(name, fixture, body) {
    this.name = name;
    this.fixture = fixture;
    this.body = body;
  }

  TestCase.prototype = {
    /**
     * The name of this test.
     * @type {string}
     */
    name: null,

    /**
     * The test fixture to set |this| to when running the test |body|.
     * @type {testing.Test}
     */
    fixture: null,

    /**
     * The test body to execute in runTest().
     * @type {Function}
     */
    body: null,

    /**
     * True when the test fixture will run the test later.
     * @type {boolean}
     * @private
     */
    deferred_: false,

    /**
     * Called at preload time, proxies to the fixture.
     * @type {Function}
     */
    preLoad: function(name) {
      if (this.fixture)
        this.fixture.preLoad();
    },

    /**
     * Called before a test runs.
     */
    setUp: function() {
      if (this.fixture)
        this.fixture.setUp();
    },

    /**
     * Called before a test is torn down (by testDone()).
     */
    tearDown: function() {
      if (this.fixture)
        this.fixture.tearDown();
    },

    /**
     * Called to run this test's body.
     */
    runTest: function() {
      if (this.body && this.fixture)
        this.fixture.runTest(this.body);
    },

    /**
     * Called after a test is run (in testDone) to test accessibility.
     */
    runAccessibilityAudit: function() {
      if (this.fixture)
        this.fixture.runAccessibilityAudit();
    },

    /**
     * Runs this test case with |this| set to the |fixture|.
     *
     * Note: Tests created with TEST_F may depend upon |this| being set to an
     * instance of this.fixture. The current implementation of TEST creates a
     * dummy constructor, but tests created with TEST should not rely on |this|
     * being set.
     * @type {Function}
     */
    run: function() {
      try {
        this.setUp();
      } catch(e) {
        // Mock4JSException doesn't inherit from Error, so fall back on
        // toString().
        console.error(e.stack || e.toString());
      }

      if (!this.deferred_)
        this.runTest();

      // tearDown called by testDone().
    },

    /**
     * Cause this TestCase to be deferred (don't call runTest()) until the
     * returned function is called.
     * @type {Function}
     * @param {WhenTestDone} whenTestDone Call testDone() at the appropriate
     *     time.
     * @param {...*} var_args Arguments to pass when running the
     *     |currentTestCase|.
     * @return {function(): void} A function thatwill run this TestCase when
     *     called.
     */
    deferRunTest: function(whenTestDone) {
      this.deferred_ = true;
      var savedArgs = new SaveMockArguments();
      var completionAction = new CallFunctionAction(
          this, savedArgs, this.runTest,
          Array.prototype.slice.call(arguments, 1));
      var runAll = new RunAllAction(
          true, whenTestDone, [completionAction]);
      return function() {
        savedArgs.arguments = Array.prototype.slice.call(arguments);
        runAll.invoke();
      };
    },

  };

  /**
   * Registry of javascript-defined callbacks for {@code chrome.send}.
   * @type {Object}
   */
  var sendCallbacks = {};

  /**
   * Registers the message, object and callback for {@code chrome.send}
   * @param {string} name The name of the message to route to this |callback|.
   * @param {Object} messageHandler Pass as |this| when calling the |callback|.
   * @param {function(...)} callback Called by {@code chrome.send}.
   * @see sendCallbacks
   */
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
   * @see overrideChrome
   */
  function registerMockMessageCallbacks(mockObject, mockClass) {
    if (!deferGlobalOverrides && !originalChrome)
      overrideChrome();
    var mockProxy = mockObject.proxy();
    for (var func in mockClass.prototype) {
      if (typeof mockClass.prototype[func] === 'function') {
        registerMessageCallback(func, mockProxy, mockProxy[func]);
      }
    }
  }

  /**
   * Holds the mapping of name -> global override information.
   * @type {Object}
   */
  var globalOverrides = {};

  /**
   * When preloading JavaScript libraries, this is true until the
   * DOMContentLoaded event has been received as globals cannot be overridden
   * until the page has loaded its JavaScript.
   * @type {boolean}
   */
  var deferGlobalOverrides = false;

  /**
   * Override the global function |funcName| with its registered mock. This
   * should not be called twice for the same |funcName|.
   * @param {string} funcName The name of the global function to override.
   */
  function overrideGlobal(funcName) {
    assertNotEquals(undefined, this[funcName]);
    var globalOverride = globalOverrides[funcName];
    assertNotEquals(undefined, globalOverride);
    assertEquals(undefined, globalOverride.original);
    globalOverride.original = this[funcName];
    this[funcName] = globalOverride.callback.bind(globalOverride.object);
  }

  /**
   * Registers the global function name, object and callback.
   * @param {string} name The name of the message to route to this |callback|.
   * @param {Object} object Pass as |this| when calling the |callback|.
   * @param {function(...)} callback Called by {@code chrome.send}.
   * @see overrideGlobal
   */
  function registerMockGlobal(name, object, callback) {
    assertEquals(undefined, globalOverrides[name]);
    globalOverrides[name] = {
      object: object,
      callback: callback,
    };

    if (!deferGlobalOverrides)
      overrideGlobal(name);
  }

  /**
   * Registers the mock API call and its function.
   * @param {string} name The '_'-separated name of the API call.
   * @param {function(...)} theFunction Mock function for this API call.
   */
  function registerMockApi(name, theFunction) {
    var path = name.split('_');

    var namespace = this;
    for(var i = 0; i < path.length - 1; i++) {
      var fieldName = path[i];
      if(!namespace[fieldName])
        namespace[fieldName] = {};

      namespace = namespace[fieldName];
    }

    var fieldName = path[path.length-1];
    namespace[fieldName] = theFunction;
  }

  /**
   * Empty function for use in making mocks.
   * @const
   */
  function emptyFunction() {}

  /**
   * Make a mock from the supplied |methodNames| array.
   * @param {Array.<string>} methodNames Array of names of methods to mock.
   * @return {Function} Constructor with prototype filled in with methods
   *     matching |methodNames|.
   */
  function makeMockClass(methodNames) {
    function MockConstructor() {}
    for(var i = 0; i < methodNames.length; i++)
      MockConstructor.prototype[methodNames[i]] = emptyFunction;
    return MockConstructor;
  }

  /**
    * Create a new class to handle |functionNames|, add method 'functions()'
    * that returns a container of standalone functions based on the mock class
    * members, and return it.
    * @return {Mock} Mock handler class.
    */
  function makeMockFunctions(functionNames) {
    var MockClass = makeMockClass(functionNames);
    var mockFunctions = mock(MockClass);
    var mockProxy = mockFunctions.proxy();

    mockFunctions.functions_ = {};

    for (var func in MockClass.prototype) {
      if (typeof MockClass.prototype[func] === 'function')
        mockFunctions.functions_[func] = mockProxy[func].bind(mockProxy);
    }

    mockFunctions.functions = function () {
      return this.functions_;
    };

    return mockFunctions;
  }

  /**
   * Register all methods of {@code mockClass.prototype} as overrides to global
   * functions of the same name as the method, using the proxy of the
   * |mockObject| to handle the functions.
   * @param {Mock4JS.Mock} mockObject The mock to register callbacks against.
   * @param {function(new:Object)} mockClass Constructor for the mocked class.
   * @see registerMockGlobal
   */
  function registerMockGlobals(mockObject, mockClass) {
    var mockProxy = mockObject.proxy();
    for (var func in mockClass.prototype) {
      if (typeof mockClass.prototype[func] === 'function')
        registerMockGlobal(func, mockProxy, mockProxy[func]);
    }
  }

  /**
   * Register all functions in |mockObject.functions()| as global API calls.
   * @param {Mock4JS.Mock} mockObject The mock to register callbacks against.
   * @see registerMockApi
   */
  function registerMockApis(mockObject) {
    var functions = mockObject.functions();
    for (var func in functions) {
      if (typeof functions[func] === 'function')
        registerMockApi(func, functions[func]);
    }
  }

  /**
   * Overrides {@code chrome.send} for routing messages to javascript
   * functions. Also falls back to sending with the original chrome object.
   * @param {string} messageName The message to route.
   */
  function send(messageName) {
    var callback = sendCallbacks[messageName];
    if (callback != undefined)
      callback[1].apply(callback[0], Array.prototype.slice.call(arguments, 1));
    else
      this.__proto__.send.apply(this.__proto__, arguments);
  }

  /**
   * Provides a mechanism for assert* and expect* methods to fetch the signature
   * of their caller. Assert* methods should |registerCall| and expect* methods
   * should set |isExpect| and |expectName| properties to indicate that the
   * interesting caller is one more level up the stack.
   */
  function CallHelper() {
    this.__proto__ = CallHelper.prototype;
  }

  CallHelper.prototype = {
    /**
     * Holds the mapping of (callerCallerString, callerName) -> count of times
     * called.
     * @type {Object.<string, Object.<string, number>>}
     */
    counts_: {},

    /**
     * This information about the caller is needed from most of the following
     * routines.
     * @param {Function} caller the caller of the assert* routine.
     * @return {{callerName: string, callercallerString: string}} stackInfo
     * @private
     */
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
     */
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
     */
    getCall_: function(caller) {
      var stackInfo = this.getCallerInfo_(caller);
      var count =
          this.counts_[stackInfo.callerCallerString][stackInfo.callerName];

      // Allow pattern to match multiple lines for text wrapping.
      var callerRegExp =
          new RegExp(stackInfo.callerName + '\\((.|\\n|\\r)*?\\);', 'g');

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
     */
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
   */
  var helper = new CallHelper();

  /**
   * true when testDone has been called.
   * @type {boolean}
   */
  var testIsDone = false;

  /**
   * Holds the errors, if any, caught by expects so that the test case can
   * fail. Cleared when results are reported from runTest() or testDone().
   * @type {Array.<Error>}
   */
  var errors = [];

  /**
   * URL to dummy WebUI page for testing framework.
   * @type {string}
   */
  var DUMMY_URL = 'chrome://DummyURL';

  /**
   * Resets test state by clearing |errors| and |testIsDone| flags.
   */
  function resetTestState() {
    errors.splice(0, errors.length);
    testIsDone = false;
  }

  /**
   * Notifies the running browser test of the test results. Clears |errors|.
   * @param {Array.<boolean, string>=} result When passed, this is used for the
   *     testResult message.
   */
  function testDone(result) {
    if (!testIsDone) {
      testIsDone = true;
      if (currentTestCase) {
        var ok = true;
        ok = createExpect(currentTestCase.runAccessibilityAudit.bind(
            currentTestCase)).call(null) && ok;
        ok = createExpect(currentTestCase.tearDown.bind(
            currentTestCase)).call(null) && ok;

        if (!ok && result)
          result = [false, errorsToMessage(errors, result[1])];

        currentTestCase = null;
      }
      if (!result)
        result = testResult();
      if (chrome.send) {
        // For WebUI tests.
        chrome.send('testResult', result);
      } else if (window.domAutomationController.send) {
        // For extension tests.
        valueResult = { 'result': result[0], message: result[1] };
        window.domAutomationController.send(JSON.stringify(valueResult));
      }
      errors.splice(0, errors.length);
    } else {
      console.warn('testIsDone already');
    }
  }

  /**
   * Converts each Error in |errors| to a suitable message, adding them to
   * |message|, and returns the message string.
   * @param {Array.<Error>} errors Array of errors to add to |message|.
   * @param {string?} message When supplied, error messages are appended to it.
   * @return {string} |message| + messages of all |errors|.
   */
  function errorsToMessage(errors, message) {
    for (var i = 0; i < errors.length; ++i) {
      var errorMessage = errors[i].stack || errors[i].message;
      if (message)
        message += '\n';

      message += 'Failed: ' + currentTestFunction + '(' +
          currentTestArguments.map(JSON.stringify) +
          ')\n' + errorMessage;
    }
    return message;
  }

  /**
   * Returns [success, message] & clears |errors|.
   * @param {boolean} errorsOk When true, errors are ok.
   * @return {Array.<boolean, string>}
   */
  function testResult(errorsOk) {
    var result = [true, ''];
    if (errors.length)
      result = [!!errorsOk, errorsToMessage(errors)];

    return result;
  }

  // Asserts.
  // Use the following assertions to verify a condition within a test.
  // If assertion fails, throw an Error with information pertinent to the test.

  /**
   * When |test| !== true, aborts the current test.
   * @param {boolean} test The predicate to check against |expected|.
   * @param {string=} message The message to include in the Error thrown.
   * @throws {Error} upon failure.
   */
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
   */
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
   */
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
   */
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
   */
  function assertEquals(expected, actual, message) {
    helper.registerCall();
    if (expected != actual) {
      throw new Error(
          'Test Error ' + helper.getCallMessage(message) +
          '\nActual: ' + actual + '\nExpected: ' + expected);
    }
    if (typeof expected !== typeof actual) {
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
   */
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
   */
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
   */
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
   */
  function assertNotReached(message) {
    helper.registerCall();
    throw new Error(helper.getCallMessage(message));
  }

  /**
   * Run an accessibility audit on the current page state.
   * @type {Function}
   * @param {Array} a11yResults
   * @param {axs.AuditConfigutarion=} opt_config
   * @return {boolean} Whether there were any errors or warnings
   * @private
   */
  function runAccessibilityAudit(a11yResults, opt_config) {
    var auditResults = axs.Audit.run(opt_config);
    for (var i = 0; i < auditResults.length; i++) {
      var auditResult = auditResults[i];
      if (auditResult.result == axs.constants.AuditResult.FAIL) {
        var auditRule = auditResult.rule;
        // TODO(aboxhall): more useful error messages (sadly non-trivial)
        a11yResults.push(auditResult);
      }
    }
    // TODO(aboxhall): have strict (no errors or warnings) vs non-strict
    // (warnings ok)
    // TODO(aboxhall): some kind of info logging for warnings only??
    return (a11yResults.length == 0);
  }

  /**
   * Concatenates the accessibility error messages for each result in
   * |a11yResults| and
   * |a11yWarnings| in to an accessibility report, appends it to the given
   * |message| and returns the resulting message string.
   * @param {Array.<string>} a11yResults The list of accessibility results
   * @return {string} |message| + accessibility report.
   */
  function accessibilityAuditReport(a11yResults, message) {
    message = message ? message + '\n\n' : '\n';
    message += 'Accessibility issues found on ' + window.location.href + '\n';
    message += axs.Audit.createReport(a11yResults);
    return message;
  }

  /**
   * Asserts that the current page state passes the accessibility audit.
   * @param {Array=} opt_results Array to fill with results, if desired.
   */
  function assertAccessibilityOk(opt_results) {
    helper.registerCall();
    var a11yResults = opt_results || [];
    var auditConfig = currentTestCase.fixture.accessibilityAuditConfig;
    if (!runAccessibilityAudit(a11yResults, auditConfig))
      throw new Error(accessibilityAuditReport(a11yResults));
  }

  /**
   * Creates a function based upon a function that thows an exception on
   * failure. The new function stuffs any errors into the |errors| array for
   * checking by runTest. This allows tests to continue running other checks,
   * while failing the overall test if any errors occurrred.
   * @param {Function} assertFunc The function which may throw an Error.
   * @return {function(...*):bool} A function that applies its arguments to
   *     |assertFunc| and returns true if |assertFunc| passes.
   * @see errors
   * @see runTestFunction
   */
  function createExpect(assertFunc) {
    var expectFunc = function() {
      try {
        assertFunc.apply(null, arguments);
      } catch (e) {
        errors.push(e);
        return false;
      }
      return true;
    };
    expectFunc.isExpect = true;
    expectFunc.expectName = assertFunc.name.replace(/^assert/, 'expect');
    return expectFunc;
  }

  /**
   * This is the starting point for tests run by WebUIBrowserTest.  If an error
   * occurs, it reports a failure and a message created by joining individual
   * error messages. This supports sync tests and async tests by calling
   * testDone() when |isAsync| is not true, relying on async tests to call
   * testDone() when they complete.
   * @param {boolean} isAsync When false, call testDone() with the test result
   *     otherwise only when assertions are caught.
   * @param {string} testFunction The function name to call.
   * @param {Array} testArguments The arguments to call |testFunction| with.
   * @return {boolean} true always to signal successful execution (but not
   *     necessarily successful results) of this test.
   * @see errors
   * @see runTestFunction
   */
  function runTest(isAsync, testFunction, testArguments) {
    // Avoid eval() if at all possible, since it will not work on pages
    // that have enabled content-security-policy.
    var testBody = this[testFunction];    // global object -- not a method.
    var testName = testFunction;

    // Depending on how we were called, |this| might not resolve to the global
    // context.
    if (testName == 'RUN_TEST_F' && testBody === undefined)
      testBody = RUN_TEST_F;

    if (typeof testBody === "undefined") {
      testBody = eval(testFunction);
      testName = testBody.toString();
    }
    if (testBody != RUN_TEST_F) {
      console.log('Running test ' + testName);
    }

    // Async allow expect errors, but not assert errors.
    var result = runTestFunction(testFunction, testBody, testArguments,
                                 isAsync);
    if (!isAsync || !result[0])
      testDone(result);
    return true;
  }

  /**
   * This is the guts of WebUIBrowserTest. It runs the test surrounded by an
   * expect to catch Errors. If |errors| is non-empty, it reports a failure and
   * a message by joining |errors|. Consumers can use this to use assert/expect
   * functions asynchronously, but are then responsible for reporting errors to
   * the browser themselves through testDone().
   * @param {string} testFunction The function name to report on failure.
   * @param {Function} testBody The function to call.
   * @param {Array} testArguments The arguments to call |testBody| with.
   * @param {boolean} onlyAssertFails When true, only assertions cause failing
   *     testResult.
   * @return {Array.<boolean, string>} [test-succeeded, message-if-failed]
   * @see createExpect
   * @see testResult
   */
  function runTestFunction(testFunction, testBody, testArguments,
                           onlyAssertFails) {
    currentTestFunction = testFunction;
    currentTestArguments = testArguments;
    var ok = createExpect(testBody).apply(null, testArguments);
    return testResult(onlyAssertFails && ok);
  }

  /**
   * Creates a new test case for the given |testFixture| and |testName|. Assumes
   * |testFixture| describes a globally available subclass of type Test.
   * @param {string} testFixture The fixture for this test case.
   * @param {string} testName The name for this test case.
   * @return {TestCase} A newly created TestCase.
   */
  function createTestCase(testFixture, testName) {
    var fixtureConstructor = this[testFixture];
    var testBody = fixtureConstructor.testCaseBodies[testName];
    var fixture = new fixtureConstructor();
    fixture.name = testFixture;
    return new TestCase(testName, fixture, testBody);
  }

  /**
   * Overrides the |chrome| object to enable mocking calls to chrome.send().
   */
  function overrideChrome() {
    if (originalChrome) {
      console.error('chrome object already overridden');
      return;
    }

    originalChrome = chrome;
    chrome = {
      __proto__: originalChrome,
      send: send,
      originalSend: originalChrome.send.bind(originalChrome),
    };
  }

  /**
   * Used by WebUIBrowserTest to preload the javascript libraries at the
   * appropriate time for javascript injection into the current page. This
   * creates a test case and calls its preLoad for any early initialization such
   * as registering handlers before the page's javascript runs it's OnLoad
   * method. This is called before the page is loaded, so the |chrome| object is
   * not yet bound and this DOMContentLoaded listener will be called first to
   * override |chrome| in order to route messages registered in |sendCallbacks|.
   * @param {string} testFixture The test fixture name.
   * @param {string} testName The test name.
   * @see sendCallbacks
   */
  function preloadJavascriptLibraries(testFixture, testName) {
    deferGlobalOverrides = true;

    // The document seems to change from the point of preloading to the point of
    // events (and doesn't fire), whereas the window does not. Listening to the
    // capture phase allows this event to fire first.
    window.addEventListener('DOMContentLoaded', function() {
      overrideChrome();

      // Override globals at load time so they will be defined.
      assertTrue(deferGlobalOverrides);
      deferGlobalOverrides = false;
      for (var funcName in globalOverrides)
        overrideGlobal(funcName);
    }, true);
    currentTestCase = createTestCase(testFixture, testName);
    currentTestCase.preLoad();
  }

  /**
   * During generation phase, this outputs; do nothing at runtime.
   */
  function GEN() {}

  /**
   * During generation phase, this outputs; do nothing at runtime.
   */
  function GEN_INCLUDE() {}

  /**
   * At runtime, register the testName with a test fixture. Since this method
   * doesn't have a test fixture, create a dummy fixture to hold its |name|
   * and |testCaseBodies|.
   * @param {string} testCaseName The name of the test case.
   * @param {string} testName The name of the test function.
   * @param {Function} testBody The body to execute when running this test.
   */
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
   */
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
   */
  function RUN_TEST_F(testFixture, testName) {
    if (!currentTestCase)
      currentTestCase = createTestCase(testFixture, testName);
    assertEquals(currentTestCase.name, testName);
    assertEquals(currentTestCase.fixture.name, testFixture);
    console.log('Running TestCase ' + testFixture + '.' + testName);
    currentTestCase.run();
  }

  /**
   * This Mock4JS matcher object pushes each |actualArgument| parameter to
   * match() calls onto |args|.
   * @param {Array} args The array to push |actualArgument| onto.
   * @param {Object} realMatcher The real matcher check arguments with.
   * @constructor
   * @extends {realMatcher}
   */
  function SaveMockArgumentMatcher(args, realMatcher) {
    this.arguments_ = args;
    this.realMatcher_ = realMatcher;
  }

  SaveMockArgumentMatcher.prototype = {
    /**
     * Holds the arguments to push each |actualArgument| onto.
     * @type {Array}
     * @private
     */
    arguments_: null,

    /**
     * The real Mock4JS matcher object to check arguments with.
     * @type {Object}
     */
    realMatcher_: null,

    /**
     * Pushes |actualArgument| onto |arguments_| and call |realMatcher_|. Clears
     * |arguments_| on non-match.
     * @param {*} actualArgument The argument to match and save.
     * @return {boolean} Result of calling the |realMatcher|.
     */
    argumentMatches: function(actualArgument) {
      this.arguments_.push(actualArgument);
      var match = this.realMatcher_.argumentMatches(actualArgument);
      if (!match)
        this.arguments_.splice(0, this.arguments_.length);

      return match;
    },

    /**
     * Proxy to |realMatcher_| for description.
     * @return {string} Description of this Mock4JS matcher.
     */
    describe: function() {
      return this.realMatcher_.describe();
    },
  };

  /**
   * Actions invoked by Mock4JS's "will()" syntax do not receive arguments from
   * the mocked method. This class works with SaveMockArgumentMatcher to save
   * arguments so that the invoked Action can pass arguments through to the
   * invoked function.
   * @param {!Object} realMatcher The real matcher to perform matching with.
   * @constructor
   */
  function SaveMockArguments() {
    this.arguments = [];
  }

  SaveMockArguments.prototype = {
    /**
     * Wraps the |realMatcher| with an object which will push its argument onto
     * |arguments| and call realMatcher.
     * @param {Object} realMatcher A Mock4JS matcher object for this argument.
     * @return {SaveMockArgumentMatcher} A new matcher which will push its
     *     argument onto |arguments|.
     */
    match: function(realMatcher) {
      return new SaveMockArgumentMatcher(this.arguments, realMatcher);
    },

    /**
     * Remember the argument passed to this stub invocation.
     * @type {Array}
     */
    arguments: null,
  };

  /**
   * CallFunctionAction is provided to allow mocks to have side effects.
   * @param {Object} obj The object to set |this| to when calling |func_|.
   * @param {?SaveMockArguments} savedArgs when non-null, saved arguments are
   *     passed to |func|.
   * @param {Function} func The function to call.
   * @param {Array=} args Any arguments to pass to func.
   * @constructor
   */
  function CallFunctionAction(obj, savedArgs, func, args) {
    this.obj_ = obj;
    this.savedArgs_ = savedArgs;
    this.func_ = func;
    this.args_ = args ? args : [];
  }

  CallFunctionAction.prototype = {
    /**
     * Set |this| to |obj_| when calling |func_|.
     * @type {?Object}
     */
    obj_: null,

    /**
     * The SaveMockArguments to hold arguments when invoking |func_|.
     * @type {?SaveMockArguments}
     * @private
     */
    savedArgs_: null,

    /**
     * The function to call when invoked.
     * @type {!Function}
     * @private
     */
    func_: null,

    /**
     * Arguments to pass to |func_| when invoked.
     * @type {!Array}
     */
    args_: null,

    /**
     * Accessor for |func_|.
     * @return {Function} The function to invoke.
     */
    get func() {
      return this.func_;
    },

    /**
     * Called by Mock4JS when using .will() to specify actions for stubs() or
     * expects(). Clears |savedArgs_| so it can be reused.
     * @return The results of calling |func_| with the concatenation of
     *     |savedArgs_| and |args_|.
     */
    invoke: function() {
      var prependArgs = [];
      if (this.savedArgs_) {
        prependArgs = this.savedArgs_.arguments.splice(
            0, this.savedArgs_.arguments.length);
      }
      return this.func.apply(this.obj_, prependArgs.concat(this.args_));
    },

    /**
     * Describe this action to Mock4JS.
     * @return {string} A description of this action.
     */
    describe: function() {
      return 'calls the given function with saved arguments and ' + this.args_;
    }
  };

  /**
   * Syntactic sugar for use with will() on a Mock4JS.Mock.
   * @param {Function} func The function to call when the method is invoked.
   * @param {...*} var_args Arguments to pass when calling func.
   * @return {CallFunctionAction} Action for use in will.
   */
  function callFunction(func) {
    return new CallFunctionAction(
        null, null, func, Array.prototype.slice.call(arguments, 1));
  }

  /**
   * Syntactic sugar for use with will() on a Mock4JS.Mock.
   * @param {SaveMockArguments} savedArgs Arguments saved with this object
   *     are passed to |func|.
   * @param {Function} func The function to call when the method is invoked.
   * @param {...*} var_args Arguments to pass when calling func.
   * @return {CallFunctionAction} Action for use in will.
   */
  function callFunctionWithSavedArgs(savedArgs, func) {
    return new CallFunctionAction(
        null, savedArgs, func, Array.prototype.slice.call(arguments, 2));
  }

  /**
   * CallGlobalAction as a subclass of CallFunctionAction looks up the original
   * global object in |globalOverrides| using |funcName| as the key. This allows
   * tests, which need to wait until a global function to be called in order to
   * start the test to run the original function. When used with runAllActions
   * or runAllActionsAsync, Mock4JS expectations may call start or continue the
   * test after calling the original function.
   * @param {?SaveMockArguments} savedArgs when non-null, saved arguments are
   *     passed to the global function |funcName|.
   * @param {string} funcName The name of the global function to call.
   * @param {Array} args Any arguments to pass to func.
   * @constructor
   * @extends {CallFunctionAction}
   * @see globalOverrides
   */
  function CallGlobalAction(savedArgs, funcName, args) {
    CallFunctionAction.call(this, null, savedArgs, funcName, args);
  }

  CallGlobalAction.prototype = {
    __proto__: CallFunctionAction.prototype,

    /**
     * Fetch and return the original global function to call.
     * @return {Function} The global function to invoke.
     * @override
     */
    get func() {
      var func = globalOverrides[this.func_].original;
      assertNotEquals(undefined, func);
      return func;
    },
  };

  /**
   * Syntactic sugar for use with will() on a Mock4JS.Mock.
   * @param {SaveMockArguments} savedArgs Arguments saved with this object
   *     are passed to the global function |funcName|.
   * @param {string} funcName The name of a registered mock global function to
   *     call when the method is invoked.
   * @param {...*} var_args Arguments to pass when calling func.
   * @return {CallGlobalAction} Action for use in Mock4JS will().
   */
  function callGlobalWithSavedArgs(savedArgs, funcName) {
    return new CallGlobalAction(
        savedArgs, funcName, Array.prototype.slice.call(arguments, 2));
  }

  /**
   * When to call testDone().
   * @enum {number}
   */
  var WhenTestDone = {
    /**
     * Default for the method called.
     */
    DEFAULT: -1,

    /**
     * Never call testDone().
     */
    NEVER: 0,

    /**
     * Call testDone() on assert failure.
     */
    ASSERT: 1,

    /**
     * Call testDone() if there are any assert or expect failures.
     */
    EXPECT: 2,

    /**
     * Always call testDone().
     */
    ALWAYS: 3,
  };

  /**
   * Runs all |actions|.
   * @param {boolean} isAsync When true, call testDone() on Errors.
   * @param {WhenTestDone} whenTestDone Call testDone() at the appropriate
   *     time.
   * @param {Array.<Object>} actions Actions to run.
   * @constructor
   */
  function RunAllAction(isAsync, whenTestDone, actions) {
    this.isAsync_ = isAsync;
    this.whenTestDone_ = whenTestDone;
    this.actions_ = actions;
  }

  RunAllAction.prototype = {
    /**
     * When true, call testDone() on Errors.
     * @type {boolean}
     * @private
     */
    isAsync_: false,

    /**
     * Call testDone() at appropriate time.
     * @type {WhenTestDone}
     * @private
     * @see WhenTestDone
     */
    whenTestDone_: WhenTestDone.ASSERT,

    /**
     * Holds the actions to execute when invoked.
     * @type {Array}
     * @private
     */
    actions_: null,

    /**
     * Runs all |actions_|, returning the last one. When running in sync mode,
     * throws any exceptions to be caught by runTest() or
     * runTestFunction(). Call testDone() according to |whenTestDone_| setting.
     */
    invoke: function() {
      try {
        var result;
        for(var i = 0; i < this.actions_.length; ++i)
          result = this.actions_[i].invoke();

        if ((this.whenTestDone_ == WhenTestDone.EXPECT && errors.length) ||
            this.whenTestDone_ == WhenTestDone.ALWAYS)
          testDone();

        return result;
      } catch (e) {
        if (!(e instanceof Error))
          e = new Error(e.toString());

        if (!this.isAsync_)
          throw e;

        errors.push(e);
        if (this.whenTestDone_ != WhenTestDone.NEVER)
          testDone();
      }
    },

    /**
     * Describe this action to Mock4JS.
     * @return {string} A description of this action.
     */
    describe: function() {
      return 'Calls all actions: ' + this.actions_;
    },
  };

  /**
   * Syntactic sugar for use with will() on a Mock4JS.Mock.
   * @param {...Object} var_actions Actions to run.
   * @return {RunAllAction} Action for use in will.
   */
  function runAllActions() {
    return new RunAllAction(false, WhenTestDone.NEVER,
                            Array.prototype.slice.call(arguments));
  }

  /**
   * Syntactic sugar for use with will() on a Mock4JS.Mock.
   * @param {WhenTestDone} whenTestDone Call testDone() at the appropriate
   *     time.
   * @param {...Object} var_actions Actions to run.
   * @return {RunAllAction} Action for use in will.
   */
  function runAllActionsAsync(whenTestDone) {
    return new RunAllAction(true, whenTestDone,
                            Array.prototype.slice.call(arguments, 1));
  }

  /**
   * Syntactic sugar for use with will() on a Mock4JS.Mock.
   * Creates an action for will() that invokes a callback that the tested code
   * passes to a mocked function.
   * @param {SaveMockArguments} savedArgs Arguments that will contain the
   *     callback once the mocked function is called.
   * @param {number} callbackParameter Index of the callback parameter in
   *     |savedArgs|.
   * @param {...Object} var_args Arguments to pass to the callback.
   * @return {CallFunctionAction} Action for use in will().
   */
  function invokeCallback(savedArgs, callbackParameter, var_args) {
    var callbackArguments = Array.prototype.slice.call(arguments, 2);
    return callFunction(function() {
      savedArgs.arguments[callbackParameter].apply(null, callbackArguments);

      // Mock4JS does not clear the saved args after invocation.
      // To allow reuse of the same SaveMockArguments for multiple
      // invocations with similar arguments, clear them here.
      savedArgs.arguments.splice(0, savedArgs.arguments.length);
    });
  }

  /**
   * Mock4JS matcher object that matches the actual argument and the expected
   * value iff their JSON represenations are same.
   * @param {Object} expectedValue
   * @constructor
   */
  function MatchJSON(expectedValue) {
    this.expectedValue_ = expectedValue;
  }

  MatchJSON.prototype = {
    /**
     * Checks that JSON represenation of the actual and expected arguments are
     * same.
     * @param {Object} actualArgument The argument to match.
     * @return {boolean} Result of the comparison.
     */
    argumentMatches: function(actualArgument) {
      return JSON.stringify(this.expectedValue_) ===
          JSON.stringify(actualArgument);
    },

    /**
     * Describes the matcher.
     * @return {string} Description of this Mock4JS matcher.
     */
    describe: function() {
      return 'eqJSON(' + JSON.stringify(this.expectedValue_) + ')';
    },
  };

  /**
   * Builds a MatchJSON argument matcher for a given expected value.
   * @param {Object} expectedValue
   * @return {MatchJSON} Resulting Mock4JS matcher.
   */
  function eqJSON(expectedValue) {
    return new MatchJSON(expectedValue);
  }

  /**
   * Mock4JS matcher object that matches the actual argument and the expected
   * value iff the the string representation of the actual argument is equal to
   * the expected value.
   * @param {string} expectedValue
   * @constructor
   */
  function MatchToString(expectedValue) {
    this.expectedValue_ = expectedValue;
  }

  MatchToString.prototype = {
    /**
     * Checks that the the string representation of the actual argument matches
     * the expected value.
     * @param {*} actualArgument The argument to match.
     * @return {boolean} Result of the comparison.
     */
    argumentMatches: function(actualArgument) {
      return this.expectedValue_ === String(actualArgument);
    },

    /**
     * Describes the matcher.
     * @return {string} Description of this Mock4JS matcher.
     */
    describe: function() {
      return 'eqToString("' + this.expectedValue_ + '")';
    },
  };

  /**
   * Builds a MatchToString argument matcher for a given expected value.
   * @param {Object} expectedValue
   * @return {MatchToString} Resulting Mock4JS matcher.
   */
  function eqToString(expectedValue) {
    return new MatchToString(expectedValue);
  }

  // Exports.
  testing.Test = Test;
  exports.testDone = testDone;
  exports.assertTrue = assertTrue;
  exports.assertFalse = assertFalse;
  exports.assertGE = assertGE;
  exports.assertGT = assertGT;
  exports.assertEquals = assertEquals;
  exports.assertLE = assertLE;
  exports.assertLT = assertLT;
  exports.assertNotEquals = assertNotEquals;
  exports.assertNotReached = assertNotReached;
  exports.assertAccessibilityOk = assertAccessibilityOk;
  exports.callFunction = callFunction;
  exports.callFunctionWithSavedArgs = callFunctionWithSavedArgs;
  exports.callGlobalWithSavedArgs = callGlobalWithSavedArgs;
  exports.eqJSON = eqJSON;
  exports.eqToString = eqToString;
  exports.expectTrue = createExpect(assertTrue);
  exports.expectFalse = createExpect(assertFalse);
  exports.expectGE = createExpect(assertGE);
  exports.expectGT = createExpect(assertGT);
  exports.expectEquals = createExpect(assertEquals);
  exports.expectLE = createExpect(assertLE);
  exports.expectLT = createExpect(assertLT);
  exports.expectNotEquals = createExpect(assertNotEquals);
  exports.expectNotReached = createExpect(assertNotReached);
  exports.expectAccessibilityOk = createExpect(assertAccessibilityOk);
  exports.invokeCallback = invokeCallback;
  exports.preloadJavascriptLibraries = preloadJavascriptLibraries;
  exports.registerMessageCallback = registerMessageCallback;
  exports.registerMockGlobals = registerMockGlobals;
  exports.registerMockMessageCallbacks = registerMockMessageCallbacks;
  exports.resetTestState = resetTestState;
  exports.runAccessibilityAudit = runAccessibilityAudit;
  exports.runAllActions = runAllActions;
  exports.runAllActionsAsync = runAllActionsAsync;
  exports.runTest = runTest;
  exports.runTestFunction = runTestFunction;
  exports.SaveMockArguments = SaveMockArguments;
  exports.DUMMY_URL = DUMMY_URL;
  exports.TEST = TEST;
  exports.TEST_F = TEST_F;
  exports.RUNTIME_TEST_F = TEST_F;
  exports.GEN = GEN;
  exports.GEN_INCLUDE = GEN_INCLUDE;
  exports.WhenTestDone = WhenTestDone;

  // Import the Mock4JS helpers.
  Mock4JS.addMockSupport(exports);
})(this);
