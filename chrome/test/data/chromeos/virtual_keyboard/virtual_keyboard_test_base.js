/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Queue for running tests asynchronously.
 */
function TestRunner(subtaskRunner) {
  this.queue = [];
  keyboard.addEventListener('stateChange', this.onStateChange.bind(this));
  this.isBlocked = false;
  this.subtaskRunner = subtaskRunner;
}

/**
 * Flag values for the shift, control and alt modifiers as defined by
 * EventFlags in "event_constants.h".
 * @type {enum}
 */
var Modifier = {
  NONE: 0,
  SHIFT: 2,
  CONTROL: 4,
  ALT: 8
};

TestRunner.prototype = {

  /**
   * Queues a test to run after the keyboard has finished initializing.
   * @param {!Function} callback The deferred function call.
   */
  append: function(callback) {
    this.queue.push(callback);
  },

  /**
   * Notification of a change in the state of the keyboard. Runs all queued
   * tests if the keyboard has finished initializing.
   * @param {Object} event The state change event.
   */
  onStateChange: function(event) {
    if (this.isBlocked)
      return;
    if (event.detail.state == 'keysetLoaded') {
      while(this.queue.length > 0) {
        var test = this.queue.shift();
        var blocks = this.runTest(test);
        if (blocks) {
          return;
        }
      }
    }
  },

  /**
   * Runs a single test, notifying the test harness of the results.
   * @param {function} callback The callback function containing the test.
   * @return {boolean} If this test blocks the testRunner.
   */
  runTest: function(callback) {
    var testFailure = false;
    try {
      // Defers any subtasks to the subtaskRunner.
      if (callback.subTasks) {
        subTaskRunner.appendAll(callback.subTasks,
                                callback.testName,
                                function(failure) {
          callback.onTestComplete(failure);
          this.unblock();
        });
        // Call the function after scheduling the subtasks since the subtasks
        // will be waiting for state changes triggered by the original callback.
        callback();
        return true;
      }
      // No subtasks, run as normal.
      callback();

    } catch(err) {
      console.error('Failure in test "' + callback.testName + '"\n' + err);
      console.log(err.stack);
      testFailure = true;
    }
    callback.onTestComplete(testFailure);
    return false;
  },

  /**
   * Unblocks the test runner and continues running tests.
   */
  unblock: function() {
    this.isBlocked = false;
    this.onStateChange({detail:{state:"keysetLoaded"}});
  }
};

/**
 * Handles subtasks that require keyset loading between each task.
 */
function SubTaskRunner() {
  this.queue = [];
  this.onCompletion = undefined;
  this.testName = undefined;
}

SubTaskRunner.prototype = {

  /**
   * Schedules subTasks to run in order. Each subTask waits for the keyboard
   * to load a new keyset before running. This is useful for tests that change
   * layouts after the original layout has been loaded.
   * @param {Array.<function>} subtasks  The subtasks to schedule.
   * @param {string} testName The name of the test being run.
   * @param {function(boolean)} onCompletion Callback function to call on
   *     completion.
   */
  appendAll: function(subtasks, testName, onCompletion) {
    this.queue = this.queue.concat(subtasks);
    this.onCompletion = onCompletion;
    this.testName = testName;
    keyboard.addEventListener('stateChange', this.onStateChange.bind(this));
  },

  /**
   * Notification of a change in the state of the keyboard. Runs the task at the
   * head of the queue in response, if the event was caused by a keysetLoaded.
   * @param {Object} event The event that triggered the state change.
   */
  onStateChange: function(event) {
    if (event.detail.state == 'keysetLoaded') {
      if (this.queue.length > 0) {
        var headSubTask = this.queue.shift();
        headSubTask();
      }
      if (this.queue.length == 0) {
        this.onCompletion(false);
        this.reset();
      }
    }
  },

  /**
   * Resets the subTaskRunner.
   */
  reset: function() {
    keyboard.removeEventListener('stateChange', this.onStateChange);
    this.onCompletion = undefined;
    this.testName = undefined;
    this.queue = [];
  },

  /**
   * Runs a single task. Aborts all subtasks if this task fails.
   * @param {function} task The task to run.
   */
  runTask: function(task) {
    try {
      task();
    } catch(err) {
      console.error('Failure in subtask of test "' +
          this.testName + '"\n' + err);
      console.log(err.stack);
      callback.onTestComplete(true);
      this.reset();
    }
  },
}

var testRunner;
var subTaskRunner;
var mockController;
var mockTimer;

/**
 * Create mocks for the virtualKeyboardPrivate API. Any tests that trigger API
 * calls must set expectations for call signatures.
 */
function setUp() {
  subTaskRunner = new SubTaskRunner();
  testRunner = new TestRunner(subTaskRunner);
  mockController = new MockController();
  mockTimer = new MockTimer();

  mockTimer.install();

  mockController.createFunctionMock(chrome.virtualKeyboardPrivate,
                                    'insertText');

  mockController.createFunctionMock(chrome.virtualKeyboardPrivate,
                                    'sendKeyEvent');

  mockController.createFunctionMock(chrome.virtualKeyboardPrivate,
                                    'hideKeyboard');

  var validateSendCall = function(index, expected, observed) {
    // Only consider the first argument (VirtualKeyEvent) for the validation of
    // sendKeyEvent calls.
    var expectedEvent = expected[0];
    var observedEvent = observed[0];
    assertEquals(expectedEvent.type,
                 observedEvent.type,
                 'Mismatched event types.');
    assertEquals(expectedEvent.charValue,
                 observedEvent.charValue,
                 'Mismatched unicode values for character.');
    assertEquals(expectedEvent.keyCode,
                 observedEvent.keyCode,
                 'Mismatched key codes.');
    assertEquals(expectedEvent.modifiers,
                 observedEvent.modifiers,
                 'Mismatched states for modifiers.');
  };
  chrome.virtualKeyboardPrivate.sendKeyEvent.validateCall = validateSendCall;

  chrome.virtualKeyboardPrivate.hideKeyboard.validateCall = function() {
    // hideKeyboard has one optional argument for error logging that does not
    // matter for the purpose of validating the call.
  };

  // TODO(kevers): Mock additional extension API calls as required.
}

/**
 * Verify that API calls match expectations.
 */
function tearDown() {
  mockController.verifyMocks();
  mockController.reset();
  mockTimer.uninstall();
}

/**
 * Finds the key on the keyboard with the matching label.
 * @param {string} label The label in the key.
 * @return {?kb-key} The key element with matching label or undefined if no
 *     matching key is found.
 */
function findKey(label) {
  var keys = keyboard.querySelectorAll('kb-key');
  for (var i = 0; i < keys.length; i++) {
    if (keys[i].charValue == label)
      return keys[i];
  }
}

/**
 * Mock typing of basic keys. Each keystroke should trigger a pair of
 * API calls to send viritual key events.
 * @param {string} label The character being typed.
 * @param {number} keyCode The legacy key code for the character.
 * @param {number} modifiers Indicates which if any of the shift, control and
 *     alt keys are being virtually pressed.
 * @param {number=} opt_unicode Optional unicode value for the character. Only
 *     required if it cannot be directly calculated from the label.
 */
function mockTypeCharacter(label, keyCode, modifiers, opt_unicode) {
  var key = findKey(label);
  assertTrue(!!key, 'Unable to find key labelled "' + label + '".');
  var unicodeValue = opt_unicode | label.charCodeAt(0);
  var send = chrome.virtualKeyboardPrivate.sendKeyEvent;
  send.addExpectation({
    type: 'keydown',
    charValue: unicodeValue,
    keyCode: keyCode,
    modifiers: modifiers
  });
  send.addExpectation({
    type: 'keyup',
    charValue: unicodeValue,
    keyCode: keyCode,
    modifiers: modifiers
  });
  var mockEvent = { pointerId:1 };
  // Fake typing the key.
  key.down(mockEvent);
  key.up(mockEvent);
}

/**
 * Triggers a callback function to run post initialization of the virtual
 * keyboard.
 * @param {string} testName The name of the test.
 * @param {Function} runTestCallback Callback function for running an
 *     asynchronous test.
 * @param {Function} testDoneCallback Callback function to indicate completion
 *     of a test.
 * @param {Array.<function>=} opt_testSubtasks Subtasks, each of which require
 *     the keyboard to load a new keyset before running.
 */
function onKeyboardReady(testName, runTestCallback, testDoneCallback,
                         opt_testSubtasks) {
  runTestCallback.testName = testName;
  runTestCallback.onTestComplete = testDoneCallback;
  runTestCallback.subTasks = opt_testSubtasks;
  if (keyboard.initialized && !testRunner.isBlocked)
    testRunner.runTest(runTestCallback);
  else
    testRunner.append(runTestCallback);
}
