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
 * A test ends when testDone is called.  This can be done by the test itself,
 * but will also be done by the test framework when an assert/expect test fails
 * or an exception is thrown.
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
   * Map of tab handle names to location hashes.
   * @type {object.<string, string>}
   */
  var hashToTabHandleIdMap = {
    capture: CaptureView.TAB_HANDLE_ID,
    export: ExportView.TAB_HANDLE_ID,
    import: ImportView.TAB_HANDLE_ID,
    proxy: ProxyView.TAB_HANDLE_ID,
    events: EventsView.TAB_HANDLE_ID,
    timeline: TimelineView.TAB_HANDLE_ID,
    dns: DnsView.TAB_HANDLE_ID,
    sockets: SocketsView.TAB_HANDLE_ID,
    spdy: SpdyView.TAB_HANDLE_ID,
    httpCache: HttpCacheView.TAB_HANDLE_ID,
    httpThrottling: HttpThrottlingView.TAB_HANDLE_ID,
    serviceProviders: ServiceProvidersView.TAB_HANDLE_ID,
    tests: TestView.TAB_HANDLE_ID,
    hsts: HSTSView.TAB_HANDLE_ID,
    logs: LogsView.TAB_HANDLE_ID,
    prerender: PrerenderView.TAB_HANDLE_ID
  };

  /**
   * Creates a test function that can use the expect and assert functions
   * in test_api.js.  Will call testDone() in response to any failure.
   *
   * The resulting function has no return value.
   * @param {string} testName The name of the function, reported on error.
   * @param {Function} testFunction The function to run.
   * @return {function():void} Function that passes its parameters to
   *     testFunction, and passes any failures to the browser by calling
   *     testDone() after they occur.
   */
  function createTestFunction(testName, testFunction) {
    return function() {
      // Convert arguments to an array, as their map method may be called on
      // failure by runTestFunction.
      var testArguments = Array.prototype.slice.call(arguments, 0);

      var result = runTestFunction(testName, testFunction, testArguments);

      // If the first value is false, report the test failure.
      if (!result[0])
        testDone();
    };
  }

  /**
   * Used to declare a test function called by the NetInternals browser test.
   * Takes in a name and a function, and creates a global test function.
   * @param {string} testName The name of the test.
   * @param {Function} testFunction The test function.
   */
  function test(testName, testFunction) {
    window[testName] = runTest.bind(null, testName, testFunction);
  }

  /**
   * Called by the browser to start a test.  If constants haven't been
   * received from the browser yet, waits until they have been.
   * Experimentally, this never seems to happen, but may theoretically be
   * possible.
   * @param {string} testName The name of the test to run.
   * @param {Function} testFunction The test function.
   */
  function runTest(testName, testFunction) {
    var testArguments = Array.prototype.slice.call(arguments, 2);

    // If we've already received the constants, start the tests.
    if (Constants) {
      startNetInternalsTest(testName, testFunction, testArguments);
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

    ConstantsObserver.prototype.onReceivedConstants = function() {
      if (!this.testStarted_) {
        this.testStarted_ = true;
        startNetInternalsTest(testFunction, testFunction, testArguments);
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
  function startNetInternalsTest(testName, testFunction, testArguments) {
    // Wrap g_browser.receive around a test function so that assert and expect
    // functions can be called from observers.
    g_browser.receive = createTestFunction('g_browser.receive', function() {
      BrowserBridge.prototype.receive.apply(g_browser, arguments);
    });

    g_browser.setPollInterval(TESTING_POLL_INTERVAL_MS);
    createTestFunction(testName, testFunction).apply(null, testArguments);
  }

  /**
   * Returns the first styled table body that's a descendent of |ancestorId|.
   * If the specified node is itself a table body node, just returns that node.
   * Returns null if no such node is found.
   * @param {string} ancestorId HTML element id containing a styled table.
   */
  function getStyledTableDescendent(ancestorId) {
    if ($(ancestorId).nodeName == 'TBODY')
      return $(ancestorId);
    // The tbody element of the first styled table in |parentId|.
    return document.querySelector('#' + ancestorId + ' .styledTable tbody');
  }

  /**
   * Finds the first styled table body that's a descendent of |ancestorId|,
   * including the |ancestorId| element itself, and returns the number of rows
   * it has. Returns -1 if there's no such table.
   * @param {string} ancestorId HTML element id containing a styled table.
   * @return {number} Number of rows the style table's body has.
   */
  function getStyledTableNumRows(ancestorId) {
    // The tbody element of the first styled table in |parentId|.
    var tbody = getStyledTableDescendent(ancestorId);
    if (!tbody)
      return -1;
    return tbody.children.length;
  }

  /**
   * Finds the first styled table body that's a descendent of |ancestorId|,
   * including the |ancestorId| element itself, and checks if it has exactly
   * |expectedRows| rows.  As only table bodies are considered, the header row
   * will not be included in the count.
   * @param {string} ancestorId HTML element id containing a styled table.
   * @param {number} expectedRows Expected number of rows in the table.
   */
  function checkStyledTableRows(ancestorId, expectedRows) {
    expectEquals(expectedRows, getStyledTableNumRows(ancestorId),
                 'Incorrect number of rows in ' + ancestorId);
  }

  /**
   * Finds the first styled table body that's a descendent of |ancestorId|,
   * including the |ancestorId| element itself, and returns the text of the
   * specified cell.  If the cell does not exist, throws an exception.
   * @param {string} ancestorId HTML element id containing a styled table.
   * @param {number} row Row of the value to retrieve.
   * @param {number} column Column of the value to retrieve.
   */
  function getStyledTableText(ancestorId, row, column) {
    var tbody = getStyledTableDescendent(ancestorId);
    return tbody.children[row].children[column].innerText;
  }

  /**
   * Returns the TabEntry with the given id.  Asserts if the tab can't be found.
   * @param {string}: tabId Id of the TabEntry to get.
   * @return {TabEntry} The specified TabEntry.
   */
  function getTab(tabId) {
    var categoryTabSwitcher = MainView.getInstance().categoryTabSwitcher();
    var tab = categoryTabSwitcher.findTabById(tabId);
    assertNotEquals(tab, undefined, tabId + ' does not exist.');
    return tab;
  }

  /**
   * Returns true if the specified tab's handle is visible, false otherwise.
   * Asserts if the handle can't be found.
   * @param {string}: tabId Id of the tab to check.
   * @return {bool} Whether or not the tab's handle is visible.
   */
  function tabHandleIsVisible(tabId) {
    var tabHandleNode = getTab(tabId).getTabHandleNode();
    return tabHandleNode.style.display != 'none';
  }

  /**
   * Returns the tab id of a tab, given its associated URL hash value.  Asserts
   *    if |hash| has no associated tab.
   * @param {string}: hash Hash associated with the tab to return the id of.
   * @return {string} String identifier of the tab with the given hash.
   */
  function getTabId(hash) {
    assertEquals(typeof hashToTabHandleIdMap[hash], 'string',
                 'Invalid tab anchor: ' + hash);
    var tabId = hashToTabHandleIdMap[hash];
    assertEquals('object', typeof getTab(tabId), 'Invalid tab: ' + tabId);
    return tabId;
  }

  /**
   * Switches to the specified tab.
   * @param {string}: hash Hash associated with the tab to switch to.
   */
  function switchToView(hash) {
    var tabId = getTabId(hash);

    // Make sure the tab handle is visible, as we only simulate normal usage.
    expectTrue(tabHandleIsVisible(tabId),
               tabId + ' does not have a visible tab handle.');
    var tabHandleNode = getTab(tabId).getTabHandleNode();

    // Simulate a left click.
    var mouseEvent = document.createEvent('MouseEvents');
    mouseEvent.initMouseEvent('click', true, true, window,
                              1, 0, 0, 0, 0,
                              false, false, false, false, 0, null);
    $(tabId).dispatchEvent(mouseEvent);

    // Make sure the hash changed.
    assertEquals('#' + hash, document.location.hash);

    // Run the onhashchange function, so we can test the resulting state.
    // Otherwise, won't trigger until after we return.
    window.onhashchange();

    // Make sure only the specified tab is visible.
    var categoryTabSwitcher = MainView.getInstance().categoryTabSwitcher();
    var tabIds = categoryTabSwitcher.getAllTabIds();
    for (var i = 0; i < tabIds.length; ++i) {
      expectEquals(tabIds[i] == tabId,
                   getTab(tabIds[i]).contentView.isVisible(),
                   tabIds[i] + ': Unexpected visibility state.');
    }
  }

  /**
   * Checks the visibility of all tab handles against expected values.
   * @param {object.<string, bool>}: tabVisibilityState Object with a an entry
   *     for each tab's hash, and a bool indicating if it should be visible or
   *     not.
   * @param {bool+}: tourTabs True if tabs expected to be visible should should
   *     each be navigated to as well.
   */
  function checkTabHandleVisibility(tabVisibilityState, tourTabs) {
    // Check visibility state of all tabs.
    var tabCount = 0;
    for (var hash in tabVisibilityState) {
      var tabId = getTabId(hash);
      assertEquals('object', typeof getTab(tabId), 'Invalid tab: ' + tabId);
      expectEquals(tabVisibilityState[hash], tabHandleIsVisible(tabId),
                   tabId + ' visibility state is unexpected.');
      if (tourTabs && tabVisibilityState[hash])
        switchToView(hash);
      tabCount++;
    }

    // Check that every tab was listed.
    var categoryTabSwitcher = MainView.getInstance().categoryTabSwitcher();
    var tabIds = categoryTabSwitcher.getAllTabIds();
    expectEquals(tabCount, tabIds.length);
  }

  /**
   * This class allows multiple Tasks to be queued up to be run sequentially.
   * A Task can wait for asynchronous callbacks from the browser before
   * completing, at which point the next queued Task will be run.
   * @param {bool}: endTestWhenDone True if testDone should be called when the
   *     final task completes.
   * @constructor
   */
  function TaskQueue(endTestWhenDone) {
    this.tasks_ = [];
    this.isRunning_ = false;
    this.endTestWhenDone_ = endTestWhenDone;
  }

  TaskQueue.prototype = {
    /**
     * Adds a Task to the end of the queue.  Each Task may only be added once
     * to a single queue.
     * @param {Task}: task The Task to add.
     */
    addTask: function(task) {
      this.tasks_.push(task);
      task.setTaskQueue_(this);
    },

    /**
     * Adds a Task to the end of the queue.  The task will call the provided
     * function, and then complete.
     * @param {function}: taskFunction The function the task will call.
     */
    addFunctionTask: function(taskFunction) {
      this.addTask(new CallFunctionTask(taskFunction));
    },

    /**
     * Starts running the Tasks in the queue.  Once called, may not be called
     * again.
     */
    run: function() {
      assertFalse(this.isRunning_);
      this.isRunning_ = true;
      this.runNextTask_();
    },

    /**
     * If there are any Tasks in |tasks_|, removes the first one and runs it.
     * Otherwise, sets |isRunning_| to false.  If |endTestWhenDone_| is true,
     * calls testDone.
     */
    runNextTask_: function() {
      assertTrue(this.isRunning_);
      if (this.tasks_.length > 0) {
        this.tasks_.shift().start();
      } else {
        this.isRunning_ = false;
        if (this.endTestWhenDone_)
          testDone();
      }
    }
  }

  /**
   * A Task that can be added to a TaskQueue.  A Task is started with a call to
   * the start function, and must call its own onTaskDone when complete.
   * @constructor
   */
  function Task() {
    this.taskQueue_ = null;
    this.isDone_ = false;
  };

  Task.prototype = {
    /**
     * Starts running the Task.  Only called once per Task, must be overridden.
     */
    start: function() {
      assertNotReached('Start function not overridden.');
    },

    /**
     * @return {bool} True if this task has completed by calling onTaskDone.
     */
    isDone: function() {
      return this.isDone_;
    },

    /**
     * Sets the TaskQueue used by the task in the onTaskDone function.  May only
     * be called by the TaskQueue.
     * @param {TaskQueue}: taskQueue The TaskQueue |this| has been added to.
     */
    setTaskQueue_: function(taskQueue) {
      assertEquals(null, this.taskQueue_);
      this.taskQueue_ = taskQueue;
    },

    /**
     * Must be called when a task is complete, and can only be called once for a
     * task.  Runs the next task, if any.
     */
    onTaskDone: function() {
      assertFalse(this.isDone_);
      this.isDone_ = true;
      this.taskQueue_.runNextTask_();
    }
  };

  /**
   * A Task that can be added to a TaskQueue.  A Task is started with a call to
   * the start function, and must call its own onTaskDone when complete.
   * @constructor
   */
  function CallFunctionTask(taskFunction) {
    Task.call(this);
    assertEquals('function', typeof taskFunction);
    this.taskFunction_ = taskFunction;
  }

  CallFunctionTask.prototype = {
    __proto__: Task.prototype,

    /**
     * Runs the function and then completes.
     */
    start: function() {
      this.taskFunction_();
      this.onTaskDone();
    }
  };

  /**
   * Returns true if a node does not have a 'display' property of 'none'.
   * @param {node}: node The node to check.
   */
  function isDisplayed(node) {
    var style = getComputedStyle(node);
    return style.getPropertyValue('display') != 'none';
  }

  /**
   * Creates a new NetLog source.  Note that the id may conflict with events
   * received from the browser.
   * @param {int}: type The source type.
   * @param {int}: id The source id.
   * @constructor
   */
  function Source(type, id) {
    assertNotEquals(getKeyWithValue(LogSourceType, type), '?');
    assertGE(id, 0);
    this.type = type;
    this.id = id;
  }

  /**
   * Creates a new NetLog event.
   * @param {Source}: source The source associated with the event.
   * @param {int}: type The event id.
   * @param {int}: time When the event occurred.
   * @param {int}: phase The event phase.
   * @param {object}: params The event parameters.  May be null.
   * @constructor
   */
  function Event(source, type, time, phase, params) {
    assertNotEquals(getKeyWithValue(LogEventType, type), '?');
    assertNotEquals(getKeyWithValue(LogEventPhase, phase), '?');

    this.source = source;
    this.phase = phase;
    this.type = type;
    this.time = "" + time;
    this.phase = phase;
    if (params)
      this.params = params;
  }

  /**
   * Creates a new NetLog begin event.  Parameters are the same as Event,
   * except there's no |phase| argument.
   * @see Event
   */
  function CreateBeginEvent(source, type, time, params) {
    return new Event(source, type, time,  LogEventPhase.PHASE_BEGIN, params);
  }

  /**
   * Creates a new NetLog end event.  Parameters are the same as Event,
   * except there's no |phase| argument.
   * @see Event
   */
  function CreateEndEvent(source, type, time, params) {
    return new Event(source, type, time,  LogEventPhase.PHASE_END, params);
  }

  /**
   * Creates a new NetLog end event matching the given begin event.
   * @param {Event}: beginEvent The begin event.  Returned event will have the
   *                 same source and type.
   * @param {int}: time When the event occurred.
   * @param {object}: params The event parameters.  May be null.
   * @see Event
   */
  function CreateMatchingEndEvent(beginEvent, time, params) {
    return CreateEndEvent(beginEvent.source, beginEvent.type, time, params);
  }

  // Exported functions.
  return {
    test: test,
    checkStyledTableRows: checkStyledTableRows,
    checkTabHandleVisibility: checkTabHandleVisibility,
    getStyledTableText: getStyledTableText,
    isDisplayed: isDisplayed,
    runTest: runTest,
    switchToView: switchToView,
    TaskQueue: TaskQueue,
    Task: Task,
    Source: Source,
    Event: Event,
    CreateBeginEvent: CreateBeginEvent,
    CreateEndEvent: CreateEndEvent,
    CreateMatchingEndEvent: CreateMatchingEndEvent
  };
})();

netInternalsTest.test('netInternalsDone', function() {
  testDone();
});

netInternalsTest.test('netInternalsExpectFail', function() {
  expectNotReached();
});

netInternalsTest.test('netInternalsAssertFail', function() {
  assertNotReached();
});

netInternalsTest.test('netInternalsObserverDone', function() {
  /**
   * A HostResolverInfo observer that calls testDone() in response to the
   * first seen event.
   */
  function HostResolverInfoObserver() {
  }

  HostResolverInfoObserver.prototype.onHostResolverInfoChanged = function() {
    testDone();
  };

  // Create the observer and add it to |g_browser|.
  g_browser.addHostResolverInfoObserver(new HostResolverInfoObserver());

  // Needed to trigger an update.
  netInternalsTest.switchToView('dns');
});

netInternalsTest.test('netInternalsObserverExpectFail', function() {
  /**
   * A HostResolverInfo observer that triggers an exception in response to the
   * first seen event.
   */
  function HostResolverInfoObserver() {
  }

  HostResolverInfoObserver.prototype.onHostResolverInfoChanged = function() {
    expectNotReached();
  };

  // Create the observer and add it to |g_browser|.
  g_browser.addHostResolverInfoObserver(new HostResolverInfoObserver());

  // Needed to trigger an update.
  netInternalsTest.switchToView('dns');
});

netInternalsTest.test('netInternalsObserverAssertFail', function() {
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
