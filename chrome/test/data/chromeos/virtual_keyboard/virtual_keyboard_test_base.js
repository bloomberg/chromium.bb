/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Set to true while debugging virtual keyboard tests, for verbose debug output.
 */
var debugging = false;

/**
 * Display diagnostic messages when debugging tests.
 * @param {string} Message to conditionally display.
 */
function Debug(message) {
  if (debugging)
    console.log(message);
}

/**
 * The enumeration of swipe directions.
 * @const
 * @enum {number}
 */
var SwipeDirection = {
  RIGHT: 0x1,
  LEFT: 0x2,
  UP: 0x4,
  DOWN: 0x8
};

/**
 * Layouts used in testing.
 * @enum {string}
 */
var Layout = {
  DEFAULT: 'qwerty',
  SYSTEM: 'system-qwerty',
  KEYPAD: 'numeric'
}

/**
 * Keysets used in testing.
 * @enum {string}
 */
var Keyset = {
  // Full names.
  DEFAULT: 'qwerty-lower',
  DEFAULT_LOWER: 'qwerty-lower',
  DEFAULT_UPPER: 'qwerty-upper',
  SYSTEM_LOWER: 'system-qwerty-lower',
  SYSTEM_UPPER: 'system-qwerty-upper',
  KEYPAD: 'numeric-symbol',
  // Shorthand names.
  LOWER: 'lower',
  UPPER: 'upper',
  SYMBOL: 'symbol',
  MORE_SYMBOLS: 'more'
};

/**
 * Key alignments.
 * @enum {string}
 */
var Alignment = {
  LEFT: 'left',
  RIGHT: 'right',
  CENTER: 'center'
};

var EventType = {
  KEY_DOWN: 'down',
  KEY_UP: 'up',
  KEY_OUT: 'out'
};


/**
 * Retrieve element with matching ID.
 * @param {string} name ID of the element.
 * @return {?Element} Matching element or null.
 */
function $(name) {
  return document.getElementById(name);
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

/**
 * Helper class for constructing a tests as a series of subtasks. Each subtask
 * is blocked on a wait condition.
 */
function SubtaskScheduler() {
  this.subtasks = [];
}

SubtaskScheduler.prototype = {
  /**
   * Specifying a layout enables shorthand names to be used for keyset IDs.
   */
  layout: null,

  /**
   * No-op initializer for a shift key test. Simply need to wait on the right
   * keyset.
   */
  init: function() {
    Debug('Running test ' + this.testName);
  },

  /**
   * Schedules a test of the shift key.
   * @param {string} testName The name of the test.
   * @param {Function} testDoneCallback The function to call on completion of
   *    the test.
   */
  scheduleTest: function(testName, testDoneCallback) {
    this.testName = testName;
    onKeyboardReady(testName, this.init.bind(this), testDoneCallback,
                    this.subtasks);
  },

  /**
   * Generates the full keyset ID.
   * @param {string} keysetId Full or partial keyset ID. The layout must be
   *     specified in the constructor if using a partial ID.
   * @return {string} Fully resolved ID of the keyset.
   */
  normalizeKeyset: function(keysetId) {
    return this.layout ? this.layout + '-' + keysetId : keysetId;
  },

  /**
   * Associates a wait condition with a task.
   * @param {Function} fn The function to decorate.
   * @param {string} keysetId The expected keyset ID at the start of the test.
   */
  addWaitCondition: function(fn, keysetId) {
    fn.waitCondition = {state: 'keysetChanged',
                        value: this.normalizeKeyset(keysetId)};
  },

  /**
   * Validates that the current keyset matches expectations.
   * @param {string} keysetId Full or partial ID of the expected keyset.
   * @param {string} message Error message.
   */
  verifyKeyset: function(keysetId, message) {
    assertEquals(this.normalizeKeyset(keysetId), $('keyboard').activeKeysetId,
        message);
  },

  /**
   * Adds a task for mocking a pause between events.
   * @param {number} duration The duration of the pause in milliseconds.
   * @param {string} keysetId Expected keyset at the start of the task.
   */
  wait: function(duration, keysetId) {
    var self = this;
    var fn = function() {
      Debug('mock wait ' + duration + 'ms');
      mockTimer.tick(duration);
    };
    this.addWaitCondition(fn, keysetId);
    this.addSubtask(fn);
  },

  /**
   * Verifies that a specific keyset is active.
   * @param {string} keysetId Name of the expected keyset.
   */
  verifyReset: function(keysetId) {
    var self = this;
    var fn = function() {
      self.verifyKeyset(keysetId, 'Unexpected keyset');
    };
    this.addWaitCondition(fn, keysetId);
    this.addSubtask(fn);
  },

  /**
   * Registers a subtask.
   * @param {Function} task The subtask to add.
   */
  addSubtask: function(task) {
    task.bind(this);
    if (!task.waitCondition)
      throw new Error('Subtask is missing a wait condition');
    this.subtasks.push(task);
  }
};

/**
 * Queue for running tests asynchronously.
 */
function TestRunner() {
  this.queue = [];
  this.activeTest = null;
  $('keyboard').addKeysetChangedObserver(this.scheduleCheckQueue.bind(this));
}

/**
 * setTimout and setInterval are mocked to facilitate testing of timed events.
 * Store off the original setTimeout method so that it may be used to queue
 * events.
 */
var safeSetTimeout = window.setTimeout;

TestRunner.prototype = {

  scheduleTest: function(callback, condition) {
    this.queue.push({
      testCallback: callback,
      waitCondition: condition
    });

    if (callback.subtasks) {
      // Store off original test complete function that reports completion on a
      // pass or fail.
      var onTestComplete = callback.onTestComplete;
      // On completion of the test setup, only mark test as complete if the test
      // failed. Subtests must finish before reporting a successful test.
      callback.onTestComplete = function(testFailure) {
        if (testFailure)
          onTestComplete(true);
      };
      var createSubtaskCompletionCallback = function(test, isLastTest) {
        return function(testFailure) {
          if (testFailure) {
            for (var i = 0; i < test.subtasks.length; i++) {
              test.subtasks[i].skipTest = true;
            }
          }
          if (testFailure || isLastTest)
            onTestComplete(testFailure);
        };
      };
      for (var i = 0; i < callback.subtasks.length; i++) {
        var task = callback.subtasks[i];
        task.onTestComplete = createSubtaskCompletionCallback(callback,
            i == callback.subtasks.length - 1);
        task.testName = callback.testName;
        this.queue.push({
          testCallback: task,
          waitCondition: task.waitCondition
        });
      }
    }
    this.checkQueue();
  },

  checkQueue: function() {
    if (!!this.activeTest)
      return;
    if (!this.queue.length)
      return;
    if (!$('keyboard').isReady()) {
      this.scheduleCheckQueue(10);
      return;
    }
    var waitCondition = this.queue[0].waitCondition;
    var keyset = $('keyboard').activeKeyset;
    var id = keyset.id;
    var continueWaiting = true;
    if (waitCondition.state == 'keysetChanged') {
      if (id == waitCondition.value) {
        continueWaiting = false;
        this.runNextTest();
      }
    } else if (waitCondition.state == 'candidatePopupVisibility') {
      var popup = keyset.querySelector('kb-altkey-container');
      if (popup && popup.hidden == !waitCondition.value) {
        continueWaiting = false;
        this.runNextTest();
      }
    } else if (waitCondition.state == 'overlayVisibility') {
      var popup =
          $('keyboard').shadowRoot.querySelector('kb-keyboard-overlay');
      if (popup && popup.hidden == !waitCondition.value) {
        continueWaiting = false;
        this.runNextTest();
      }
    }
    if (continueWaiting) {
      Debug('waiting on ' + waitCondition.state + ' = ' +
          waitCondition.value);
    }
  },

  runNextTest: function() {
    var testFailure = false;
    var callback = this.queue[0].testCallback;
    this.queue.shift();
    try {
      this.activeTest = callback;
      if (!callback.skipTest)
        callback();
    } catch(err) {
      console.error('Failure in test "' + callback.testName + '"\n' + err);
      console.log(err.stack);
      testFailure = true;
    }
    this.activeTest = null;
    if (!callback.skipTest)
      callback.onTestComplete(testFailure);
    this.scheduleCheckQueue();
  },

  /**
   * Schedule an asynchronous check for an unblocked task on the queue.
   * @param {number=} opt_delay Optional delay in milliseconds.
   */
  scheduleCheckQueue: function(opt_delay) {
    var delay = opt_delay;
    if (!delay)
      delay = 0;
    var self = this;
    safeSetTimeout(function() {
      self.checkQueue();
    }, delay);
  }
};

var testRunner;
var mockController;
var mockTimer;

/**
 * Create mocks for the virtualKeyboardPrivate API. Any tests that trigger API
 * calls must set expectations for call signatures.
 */
function setUp() {
  testRunner = new TestRunner();
  mockController = new MockController();
  mockTimer = new MockTimer();

  mockTimer.install();

  mockController.createFunctionMock(chrome.virtualKeyboardPrivate,
                                    'insertText');

  mockController.createFunctionMock(chrome.virtualKeyboardPrivate,
                                    'sendKeyEvent');

  mockController.createFunctionMock(chrome.virtualKeyboardPrivate,
                                    'lockKeyboard');
  mockController.createFunctionMock(chrome.virtualKeyboardPrivate,
                                    'hideKeyboard');
  mockController.createFunctionMock(chrome.virtualKeyboardPrivate,
                                    'moveCursor');

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

  var validateLockKeyboard = function(index, expected, observed) {
    assertEquals(expected[0],
                 observed[0],
                 'Mismatched keyboard lock/unlock state.');
  };
  chrome.virtualKeyboardPrivate.lockKeyboard.validateCall =
      validateLockKeyboard;

  chrome.virtualKeyboardPrivate.hideKeyboard.validateCall = function() {
    // hideKeyboard has one optional argument for error logging that does not
    // matter for the purpose of validating the call.
  };

  var validateMoveCursor = function(index, expected, observed) {
    assertEquals(expected[0], observed[0], 'Mismatched swipe directions.');
    assertEquals(expected[1], observed[1], 'Mismatched swipe flags.');
  };
  chrome.virtualKeyboardPrivate.moveCursor.validateCall = validateMoveCursor;

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
  var keyset = $('keyboard').activeKeyset;
  var keys = keyset.querySelectorAll('kb-key');
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
  var mockEvent = {pointerId: 1};
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
 * @param {Array.<Function>=} opt_testSubtasks Subtasks, each of which require
 *     the keyboard to load a new keyset before running.
 */
function onKeyboardReady(testName, runTestCallback, testDoneCallback,
                         opt_testSubtasks) {
  waitOnKeyset(Keyset.DEFAULT, testName, runTestCallback, testDoneCallback,
               opt_testSubtasks);
}

/**
 * Triggers a callback function to run once a keyset becomes active.
 * @parma {string} keysetName Full name of the target keyset.
 * @param {string} testName The name of the test.
 * @param {Function} runTestCallback Callback function for running an
 *     asynchronous test.
 * @param {Function} testDoneCallback Callback function to indicate completion
 *     of a test.
 * @param {Array.<function>=} opt_testSubtasks Subtasks, each of which require
 *     the keyboard to load a new keyset before running.
 */
function waitOnKeyset(keysetName, testName, runTestCallback, testDoneCallback,
                      opt_testSubtasks) {
  runTestCallback.testName = testName;
  runTestCallback.onTestComplete = testDoneCallback;
  runTestCallback.subtasks = opt_testSubtasks;
  testRunner.scheduleTest(runTestCallback,
                          {state: 'keysetChanged', value: keysetName});
}
