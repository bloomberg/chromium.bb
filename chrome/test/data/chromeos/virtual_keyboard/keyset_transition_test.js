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
  MORE_SYMBOLS: '~[<',
  SHIFT: 'shift',
  SYMBOL: '?123',
  TEXT: 'ABC'
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
   * Extends the subtask scheduler.
   */
  __proto__: SubtaskScheduler.prototype,

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
        if (candidates[i].innerText == key ||
            candidates[i].classList.contains(key)) {
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
   * Adds a task for mocking a key typed event.
   * @param {string} key Text label on the key.
   * @param {number} keyCode The legacy key code for the character.
   * @param {number} modifiers Indicates which if any of the shift, control and
   *     alt keys are being virtually pressed.
   * @param {string} keysetId Name of the expected keyset at the start of the
   *     task.
   */
  typeKey: function(key, keyCode, modifiers, keysetId) {
    var self = this;
    var fn = function () {
      Debug('Generating key typed event for key = ' + key + " and keycode = "
          + keyCode);
      self.verifyKeyset(keysetId, 'Unexpected keyset.');
      mockTypeCharacter(key, keyCode, modifiers)
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
  }
};

/**
 * Tests that capitalizion persists on keyset transitions.
 * The test is run asynchronously since the keyboard loads keysets dynamically.
 * @param {Function} testDoneCallback The function to be called on completion.
 */
function testPersistantCapitalizationAsync(testDoneCallback) {
  var tester = new KeysetTransitionTester(Layout.DEFAULT);

  /**
   * Checks persistant capitalization using the shift key with the given
   * alignment.
   * @param {string} alignment The alignment of the shift key.
   */
  var checkPersistantCapitalization = function(alignment) {
    // Shift-lock
    tester.keyEvent(alignment, Key.SHIFT, EventType.KEY_DOWN, Keyset.LOWER);
    tester.wait(1000, Keyset.UPPER);
    tester.keyEvent(alignment, Key.SHIFT, EventType.KEY_UP, Keyset.UPPER);

     // text -> symbol -> more -> text
    tester.keyEvent(Alignment.CENTER, Key.SYMBOL, EventType.KEY_DOWN,
        Keyset.UPPER);
    tester.keyEvent(Alignment.CENTER, Key.TEXT, EventType.KEY_UP,
        Keyset.SYMBOL);
    tester.keyEvent(Alignment.CENTER, Key.MORE_SYMBOLS, EventType.KEY_DOWN,
        Keyset.SYMBOL);
    tester.keyEvent(Alignment.CENTER, Key.SYMBOL, EventType.KEY_UP,
        Keyset.MORE_SYMBOLS);
    tester.keyEvent(Alignment.CENTER, Key.TEXT, EventType.KEY_DOWN,
        Keyset.MORE_SYMBOLS);
    // Symbol key only has center alignment.
    tester.keyEvent(Alignment.CENTER, Key.SYMBOL, EventType.KEY_UP,
        Keyset.UPPER);

    // switch back to lower case
    tester.keyEvent(alignment, Key.SHIFT, EventType.KEY_DOWN,
        Keyset.UPPER);
    tester.keyEvent(alignment, Key.SHIFT, EventType.KEY_UP, Keyset.LOWER);
  };
  // Run the test using the left shift key.
  checkPersistantCapitalization(Alignment.LEFT);
  // Run the test using the right shift key.
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

  /**
   * Checks the transitions using the shift key with the given alignment.
   * @param {string} alignment The alignment of the shift key.
   */
  var checkBasicTransitions = function(alignment) {
    // Test the path abc -> symbol -> more -> symbol -> abc.
    tester.keyEvent(Alignment.CENTER, Key.SYMBOL, EventType.KEY_DOWN,
        Keyset.LOWER);
    tester.keyEvent(Alignment.CENTER, Key.TEXT, EventType.KEY_UP,
        Keyset.SYMBOL);
    tester.keyEvent(Alignment.CENTER, Key.MORE_SYMBOLS, EventType.KEY_DOWN,
        Keyset.SYMBOL);
    tester.keyEvent(Alignment.CENTER, Key.SYMBOL, EventType.KEY_UP,
        Keyset.MORE_SYMBOLS);
    tester.keyEvent(Alignment.CENTER, Key.SYMBOL, EventType.KEY_DOWN,
        Keyset.MORE_SYMBOLS);
    tester.keyEvent(Alignment.CENTER, Key.MORE_SYMBOLS, EventType.KEY_UP,
        Keyset.SYMBOL);
    tester.keyEvent(Alignment.CENTER, Key.TEXT, EventType.KEY_DOWN,
        Keyset.SYMBOL);
    tester.keyEvent(Alignment.CENTER, Key.SYMBOL, EventType.KEY_UP,
        Keyset.LOWER);

    // Test the path abc -> symbol -> more -> abc.
    tester.keyEvent(Alignment.CENTER, Key.SYMBOL, EventType.KEY_DOWN,
        Keyset.LOWER);
    // Mock keyUp on the abc since it occupies the space where the
    // symbol key used to be.
    tester.keyEvent(Alignment.CENTER, Key.TEXT, EventType.KEY_UP,
        Keyset.SYMBOL);
    tester.keyEvent(Alignment.CENTER, Key.MORE_SYMBOLS, EventType.KEY_DOWN,
        Keyset.SYMBOL);
    tester.keyEvent(Alignment.CENTER, Key.SYMBOL, EventType.KEY_UP,
        Keyset.MORE_SYMBOLS);
    tester.keyEvent(Alignment.CENTER, Key.TEXT, EventType.KEY_DOWN,
        Keyset.MORE_SYMBOLS);
    tester.keyEvent(Alignment.CENTER, Key.SYMBOL, EventType.KEY_UP,
        Keyset.LOWER);

    // Test the path abc ->  highlighted ABC -> symbol -> abc.
    tester.keyEvent(alignment, Key.SHIFT, EventType.KEY_DOWN, Keyset.LOWER);
    tester.keyEvent(alignment, Key.SHIFT, EventType.KEY_UP, Keyset.UPPER);
    tester.keyEvent(Alignment.CENTER, Key.SYMBOL, EventType.KEY_DOWN,
        Keyset.UPPER);
    tester.keyEvent(Alignment.CENTER, Key.TEXT, EventType.KEY_UP,
        Keyset.SYMBOL);
    tester.keyEvent(Alignment.CENTER, Key.TEXT, EventType.KEY_DOWN,
        Keyset.SYMBOL);
    tester.keyEvent(Alignment.CENTER, Key.SYMBOL, EventType.KEY_UP,
        Keyset.LOWER);
  };
  // Tests the transitions using the left shift key.
  checkBasicTransitions(Alignment.LEFT);
  // Tests the transitions using the right shift key.
  checkBasicTransitions(Alignment.RIGHT);

  tester.scheduleTest('testKeysetTransitionsAsync', testDoneCallback);
}

/**
 * Tests that we transition to uppercase on punctuation followed by a space.
 * The test is run asynchronously since the keyboard loads keysets dynamically.
 * @param {Function} testDoneCallback The function to be called on completion.
 */
function testUpperOnSpaceAfterPunctuation(testDoneCallback) {
  var tester = new KeysetTransitionTester('qwerty');
  tester.typeKey('a', 0x41, Modifier.NONE, Keyset.LOWER);
  tester.typeKey('.', 0xBE, Modifier.NONE, Keyset.LOWER);
  tester.typeKey(' ', 0x20, Modifier.NONE, Keyset.LOWER);
  tester.typeKey('A', 0x41, Modifier.SHIFT, Keyset.UPPER);
  tester.typeKey('a', 0x41, Modifier.NONE, Keyset.LOWER);
  // Test that we do not enter upper if there is a character between the . and
  // the space.
  tester.typeKey('.', 0xBE, Modifier.NONE, Keyset.LOWER);
  tester.typeKey('a', 0x41, Modifier.NONE, Keyset.LOWER);
  tester.typeKey(' ', 0x20, Modifier.NONE, Keyset.LOWER);
  tester.typeKey('a', 0x41, Modifier.NONE, Keyset.LOWER);
  // Test the newline also causes a transition to upper.
  tester.typeKey('a', 0x41, Modifier.NONE, Keyset.LOWER);
  tester.typeKey('.', 0xBE, Modifier.NONE, Keyset.LOWER);
  tester.typeKey('\n', 0x0D, Modifier.NONE, Keyset.LOWER);
  tester.typeKey('A', 0x41, Modifier.SHIFT, Keyset.UPPER);
  tester.typeKey('a', 0x41, Modifier.NONE, Keyset.LOWER);
  tester.scheduleTest('testUpperOnSpaceAfterPunctuation', testDoneCallback);
}
