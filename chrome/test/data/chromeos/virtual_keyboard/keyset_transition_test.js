/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Special keys for toggling keyset transitions.
 * @enum {string}
 */
var Key = {
  MORE_SYMBOLS: 'more',
  SHIFT: 'shift',
  SYMBOL: '#123',
  TEXT: 'abc'
};

/**
 * Input types.
 * @enum {string}
 */
var InputType = {
  TEXT: 'text',
  NUMBER: 'number'
}

/**
 * Tester for keyset transitions.
 * @param {string=} opt_layout Optional layout. Used to generate the full
 *     keyset ID from its shorthand name. If not specified, then the full
 *     keyset ID is required for the test.
 * @constructor
 */
function KeysetTransitionTester(opt_layout) {
  this.layout = opt_layout;
  this.subtasks = [];
};

KeysetTransitionTester.prototype = {
  /**
   * No-op initializer for a keyset transition test. Simply need to wait on the
   * right keyset.
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
   */
  normalizeKeyset: function(keysetId) {
    return this.layout ? this.layout + '-' + keysetId : keysetId;
  },

  /**
   * Validates that the current keyset matches expectations.
   * @param {string} keysetId Full or partial ID of the expected keyset.
   * @parma {string} message Error message.
   */
  verifyKeyset: function(keysetId, message) {
    assertEquals(this.normalizeKeyset(keysetId), $('keyboard').activeKeysetId,
        message);
  },

  /**
   * Associates a wait condition with as task.
   */
  addWaitCondition: function(fn, keysetId) {
    fn.waitCondition = {state: 'keysetChanged',
                        value: this.normalizeKeyset(keysetId)};
  },

  /**
   * Adds a task for mocking a key event.
   * @param {string} aligment Indicates if the left or right version of the
   *     keyset transition key should be used.
   * @param {string} key Text label on the key.
   * @param {string} eventType Name of the event.
   * @param {string} keysetId Name of the expected keyset at the start of the
   *     task.
   */
  keyEvent: function(alignment, key, eventType, keysetId) {
    var self = this;
    var fn = function() {
      Debug('Generating key event: alignment = ' + alignment + ', key = ' + key
          + ', event type = ' + eventType);
      self.verifyKeyset(keysetId, 'Unexpected keyset.');
      var keyset = $('keyboard').activeKeyset;
      assertTrue(!!keyset, 'Missing active keyset');
      var search  = '[align="' + alignment + '"]';
      var candidates = keyset.querySelectorAll(search).array();
      for (var i = 0; i < candidates.length; i++) {
        if (candidates[i].innerText == key) {
          candidates[i][eventType]({pointerId: 1,  isPrimary:true});
          return;
        }
      }
      throw new Error('Unable to find \'' + key + '\' key in ' + keysetId);
    };
    this.addWaitCondition(fn, keysetId);
    this.addSubtask(fn);
  },

  /**
   * Adds a task for mocking a pause between events.
   * @param {number} duration The duration of the pause in milliseconds.
   * @param {string} keysetId Expected keyset at the start of the task.
   */
  wait: function(duration, keysetId) {
    var self = this;
    var fn = function() {
      mockTimer.tick(duration);
    };
    this.addWaitCondition(fn, keysetId);
    this.addSubtask(fn);
  },

  /**
   * Updates the input type.
   * @param {string} inputType The new input type.
   * @param {string} keysetId Expected keyset at the start of the task.
   */
  transition: function(inputType, keysetId) {
    var self = this;
    var fn = function() {
      self.verifyKeyset(keysetId, 'Unexpected keyset');
      Debug('changing input type to ' + inputType);
      $('keyboard').inputTypeValue = inputType;
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
  },
};

/**
 * Tests that capitalizion persists on keyset transitions.
 * The test is run asynchronously since the keyboard loads keysets dynamically.
 * @param {Function} testDoneCallback The function to be called on completion.
 */
function testPersistantCapitalizationAsync(testDoneCallback) {
  var tester = new KeysetTransitionTester(Layout.DEFAULT);

  var checkPersistantCapitalization = function(alignment) {
    // Shift-lock
    tester.keyEvent(alignment, Key.SHIFT, EventType.KEY_DOWN, Keyset.LOWER);
    tester.wait(1000, Keyset.UPPER);
    tester.keyEvent(alignment, Key.SHIFT, EventType.KEY_UP, Keyset.UPPER);

     // text -> symbol -> more -> text
    tester.keyEvent(alignment, Key.SYMBOL, EventType.KEY_DOWN, Keyset.UPPER);
    tester.keyEvent(alignment, Key.TEXT, EventType.KEY_UP, Keyset.SYMBOL);
    tester.keyEvent(alignment, Key.MORE_SYMBOLS, EventType.KEY_DOWN,
        Keyset.SYMBOL);
    tester.keyEvent(alignment, Key.SYMBOL, EventType.KEY_UP,
        Keyset.MORE_SYMBOLS);
    tester.keyEvent(alignment, Key.TEXT, EventType.KEY_DOWN,
        Keyset.MORE_SYMBOLS);
    tester.keyEvent(alignment, Key.SYMBOL, EventType.KEY_UP, Keyset.UPPER);

    // switch back to lower case
    tester.keyEvent(alignment, Key.SHIFT, EventType.KEY_DOWN, Keyset.UPPER);
    tester.keyEvent(alignment, Key.SHIFT, EventType.KEY_UP, Keyset.LOWER);
  };
  checkPersistantCapitalization(Alignment.LEFT);
  checkPersistantCapitalization(Alignment.RIGHT);

  tester.scheduleTest('testInputTypeResponsivenessAsync', testDoneCallback);
}

/**
 * Tests that changing the input type changes the layout. The test is run
 * asynchronously since the keyboard loads keysets dynamically.
 * @param {Function} testDoneCallback The function to be called on completion.
 */
function testInputTypeResponsivenessAsync(testDoneCallback) {
  var tester = new KeysetTransitionTester();

  tester.init = function() {
    $('keyboard').inputTypeValue = 'text';
  };

  // Shift-lock
  tester.keyEvent(Alignment.LEFT, Key.SHIFT, EventType.KEY_DOWN,
      Keyset.DEFAULT_LOWER);
  tester.wait(1000, Keyset.DEFAULT_UPPER);
  tester.keyEvent(Alignment.LEFT, Key.SHIFT, EventType.KEY_UP,
      Keyset.DEFAULT_UPPER);

  // Force keyset tranistions via input type changes. Should reset to lowercase
  // once back to the text keyset.
  tester.transition(InputType.NUMBER, Keyset.DEFAULT_UPPER);
  tester.transition(InputType.TEXT, Keyset.KEYPAD);
  tester.verifyReset(Keyset.DEFAULT_LOWER);

  tester.scheduleTest('testInputTypeResponsivenessAsync', testDoneCallback);
}

/**
 * Tests that keyset transitions work as expected.
 * The test is run asynchronously since the keyboard loads keysets dynamically.
 * @param {Function} testDoneCallback The function to be called on completion.
 */
function testKeysetTransitionsAsync(testDoneCallback) {

  var tester = new KeysetTransitionTester('qwerty');

  var checkBasicTransitions = function(alignment) {
    // Test the path abc -> symbol -> more -> symbol -> abc.
    tester.keyEvent(alignment, Key.SYMBOL, EventType.KEY_DOWN, Keyset.LOWER);
    tester.keyEvent(alignment, Key.TEXT, EventType.KEY_UP, Keyset.SYMBOL);
    tester.keyEvent(alignment, Key.MORE_SYMBOLS, EventType.KEY_DOWN,
        Keyset.SYMBOL);
    tester.keyEvent(alignment, Key.SYMBOL, EventType.KEY_UP,
        Keyset.MORE_SYMBOLS);
    tester.keyEvent(alignment, Key.SYMBOL, EventType.KEY_DOWN,
        Keyset.MORE_SYMBOLS);
    tester.keyEvent(alignment, Key.MORE_SYMBOLS, EventType.KEY_UP,
        Keyset.SYMBOL);
    tester.keyEvent(alignment, Key.TEXT, EventType.KEY_DOWN, Keyset.SYMBOL);
    tester.keyEvent(alignment, Key.SYMBOL, EventType.KEY_UP, Keyset.LOWER);

    // Test the path abc -> symbol -> more -> abc.
    tester.keyEvent(alignment, Key.SYMBOL, EventType.KEY_DOWN, Keyset.LOWER);
    tester.keyEvent(alignment, Key.TEXT, EventType.KEY_UP, Keyset.SYMBOL);
    tester.keyEvent(alignment, Key.MORE_SYMBOLS, EventType.KEY_DOWN,
        Keyset.SYMBOL);
    tester.keyEvent(alignment, Key.SYMBOL, EventType.KEY_UP,
        Keyset.MORE_SYMBOLS);
    tester.keyEvent(alignment, Key.TEXT, EventType.KEY_DOWN,
        Keyset.MORE_SYMBOLS);
    tester.keyEvent(alignment, Key.SYMBOL, EventType.KEY_UP, Keyset.LOWER);
  };

  checkBasicTransitions(Alignment.LEFT);
  checkBasicTransitions(Alignment.RIGHT);

  tester.scheduleTest('testKeysetTransitionsAsync', testDoneCallback);
}
