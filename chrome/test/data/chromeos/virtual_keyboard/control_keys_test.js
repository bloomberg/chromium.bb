/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Class for testing keyset transitions resulting from use of the left or right
 * shift key. Keyset transitions are asynchronous, and each test needs to be
 * broken into subtasks in order to synchronize with keyset updates.
 * @param {string=} opt_layout Optional name of the initial layout.
 * @param {string=} opt_unlocked Optional short-name of the lowercase keyset.
 * @param {string=} opt_locked Optional short-name of the uppercase keyset.
 * @constructor
 */
function ShiftKeyTester(opt_layout, opt_unlocked, opt_locked) {
  this.layout = opt_layout || 'qwerty';
  this.unlocked = opt_unlocked || 'lower';
  this.locked = opt_locked || 'upper';
  this.subtasks = [];
}

ShiftKeyTester.prototype = {
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
   * @param {function} testDoneCallback The function to call on completion of
   *    the test.
   */
  scheduleTest: function(testName, testDoneCallback) {
    this.testName = testName;
    onKeyboardReady(testName, this.init.bind(this), testDoneCallback,
                    this.subtasks);
  },

  /**
   * Verifies that the current keyset matches the expected keyset.
   * @param {boolean} isLocked Indicates if the keyset correspond to uppercase
   *     characters.
   * @param {string} message The message to display on an error.
   */
  verifyLockState: function(isLocked, message) {
    var keyset = isLocked ? this.locked : this.unlocked;
    assertEquals(keyset, $('keyboard').keyset, message);
    var keys = $('keyboard').activeKeyset.querySelectorAll('kb-shift-key');
    this.shiftKey = keys[this.alignment == 'left' ? 0 : 1];
  },

  /**
   * Sets the condition for starting the subtask. Execution of the test is
   * blocked until the wait condition is satisfied.
   * @param {!Function} fn The function to decorate with a wait condition.
   * @param {boolean} isLocked Indicates if the test is blocked on an uppercase
   *     keyset.
   */
  addWaitCondition: function(fn, isLocked) {
    var self = this;
    fn.waitCondition = {
      state: 'keysetChanged',
      value: self.layout + '-' + (isLocked ? self.locked : self.unlocked)
    };
  },

  /**
   * Adds a subtask for mocking a single key event on a shift key.
   * @param {string} alignment Indicates if triggering on the left or right
   *     shift key.
   * @param {string} keyOp Name of the event.
   * @param {boolean} isLocked Indicates the expected keyset prior to the key
   *     event.
   * @param {boolean=} opt_chording Optional parameter to indicate that the key
   *     event is for a chording operation.
   */
  keyEvent: function(alignment, keyOp, isLocked, opt_chording) {
    var self = this;
    var fn = function() {
      Debug('Mocking shift key event: alignment = ' + alignment + ', keyOp = ' +
          keyOp + ', isLocked = ' + isLocked + ', chording = ' +
          (opt_chording ? true : false));
      self.alignment = alignment;
      self.verifyLockState(isLocked,
          'Unexpected keyset before shift key event.');
      var mockKeyEvent = {pointerId: 2, isPrimary:true, target: self.shiftKey};
      self.shiftKey[keyOp](mockKeyEvent);
      // An 'up" event needs to fire a pointer up on the keyboard in order to
      // handle click counting.
      if (keyOp == 'up')
        $('keyboard').up(mockKeyEvent);
      if (opt_chording)
        self.shiftKey.out(mockKeyEvent);
    };
    this.addWaitCondition(fn, isLocked);
    this.addSubtask(fn);
  },

  /**
   * Adds a subtask for mocking a pause between events.
   * @param {number} duration The duration of the pause in milliseconds.
   * @parma {boolean} isLocked Indicates the expected keyset prior to the pause.
   */
  wait: function(duration, isLocked) {
    var self = this;
    var fn = function() {
      mockTimer.tick(duration);
    };
    this.addWaitCondition(fn, isLocked);
    this.addSubtask(fn);
  },

  /**
   * Adds a subtask for mocking the typing of a lowercase or uppercase 'A'.
   * @param {boolean} isUppercase Indicates the case of the character to type.
   */
  typeCharacter: function(isUppercase) {
    var self = this;
    var fn = function() {
      Debug('Mock typing ' + (isUppercase ? 'A' : 'a'));
      self.verifyLockState(isUppercase,
          'Unexpected keyset for typed character.');
      mockTypeCharacter(isUppercase ? 'A' : 'a', 0x41,
                        isUppercase ? Modifier.SHIFT : Modifier.NONE);
    };
    this.addWaitCondition(fn, isUppercase);
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
 * Retrieves the left or right shift keys. Shift keys come in pairs for upper
 * and lowercase keyboard layouts respectively.
 * @param {string} alignment Which of {left, right} shift keys to return.
 * @return {Object.<string: Object>} a map from keyset to the shift key in it.
 */
function getShiftKeys(alignment) {
  var layout = $('keyboard').layout;
  var keysets = ['lower', 'upper'];

  var result={};
  for(var keysetIndex in keysets) {
    var keysetId = keysets[keysetIndex];
    // The keyset DOM object which contains a shift key.
    var keyset = $('keyboard').querySelector('#' + layout + "-" + keysetId);
    assertTrue(!!keyset, "Cannot find keyset: " + keyset);
    var shiftKey = keyset.querySelector('kb-shift-key[align="' +
        alignment + '"]');
    assertTrue(!!shiftKey, "Keyset " + keysetId +
        " does not have a shift key with " + alignment + " alignment");
    result[keysetId] = shiftKey;
  }
  return result;
}

/**
 * Retrieves the specified modifier key from the system-qwerty layout.
 * @param {string} modifier Which modifier key to return.
 * @return {Object} The modifier key.
 */
function getModifierKey(modifier) {
  var keyset = $('keyboard').activeKeyset;
  assertTrue(!!keyset, 'Cannot find active keyset.');
  var modifierKey = keyset.querySelector('kb-modifier-key[char="' +
      modifier + '"]');
  assertTrue(!!modifierKey, "Modifier key not found: " + modifierKey);
  return modifierKey;
}

/**
 * Tests that a particular shift key has been initialized correctly.
 * @param {Object} shift The shift key.
 */
function checkShiftInitialState(shift) {
  //Checks that the unlocked case is lower.
  var unlocked = 'lower';
  assertEquals(unlocked, shift.lowerCaseKeysetId,
      "Mismatched lowerCaseKeysetId.");
  //Checks that the locked case is upper.
  var locked = 'upper';
  assertEquals(locked, shift.upperCaseKeysetId,
      "Mismatched upperCaseKeysetId.");
}

/**
 * Asynchronously tests highlighting of the left and right shift keys.
 * @param {function} testDoneCallBack The function to be called
 * on completion.
 */
function testShiftHighlightAsync(testDoneCallback) {
  var tester = new ShiftKeyTester();

  var checkShiftTap = function(alignment) {
    tester.keyEvent(alignment, EventType.KEY_DOWN, false);
    tester.keyEvent(alignment, EventType.KEY_UP, true);
    tester.typeCharacter(true);
    tester.typeCharacter(false);
  };
  checkShiftTap(Alignment.LEFT);
  checkShiftTap(Alignment.RIGHT);

  tester.scheduleTest('testShiftKeyHighlightAsync', testDoneCallback);
}

/**
 * Asynchronously tests initialization of the left and right shift keys.
 * @param {function} testDoneCallBack The function to be called
 * on completion.
 */
function testShiftKeyInitAsync(testDoneCallback) {
  var runTest = function() {
    var alignments = ['left', 'right'];
    for (var i in alignments) {
      var alignment = alignments[i];
      var shifts = getShiftKeys(alignment);
      checkShiftInitialState(shifts['lower']);
      checkShiftInitialState(shifts['upper']);
   }
  };
  onKeyboardReady('testShiftKeyInitAsync', runTest, testDoneCallback);
}

/**
 * Asynchronously tests capitalization on double click of the left and
 * right shift keys.
 * @param {function} testDoneCallBack The function to be called
 * on completion.
 */
function testShiftDoubleClickAsync(testDoneCallback) {
  var tester = new ShiftKeyTester();

  var checkShiftDoubleClick = function(alignment) {
    tester.keyEvent(alignment, EventType.KEY_DOWN, false);
    tester.keyEvent(alignment, EventType.KEY_UP, true);
    tester.keyEvent(alignment, EventType.KEY_DOWN, true);
    tester.keyEvent(alignment, EventType.KEY_UP, true);

    tester.typeCharacter(true);
    tester.typeCharacter(true);

    tester.keyEvent(alignment, EventType.KEY_DOWN, true);
    tester.keyEvent(alignment, EventType.KEY_UP, false);

    tester.typeCharacter(false);
  };
  checkShiftDoubleClick(Alignment.LEFT);
  checkShiftDoubleClick(Alignment.RIGHT);

  tester.scheduleTest('testShiftDoubleClickAsync', testDoneCallback);
}

/**
 * Asynchronously tests capitalization on long press of the left and
 * right shift keys.
 * @param {function} testDoneCallBack The callback function to be called
 * on completion.
 */
function testShiftLongPressAsync(testDoneCallback) {
  var tester = new ShiftKeyTester();

  var checkShiftLongPress = function(alignment) {
    tester.keyEvent(alignment, EventType.KEY_DOWN, false);
    tester.wait(1000, true);
    tester.keyEvent(alignment, EventType.KEY_UP, true);

    tester.typeCharacter(true);
    tester.typeCharacter(true);

    tester.keyEvent(alignment, EventType.KEY_DOWN, true);
    tester.keyEvent(alignment, EventType.KEY_UP, false);

    tester.typeCharacter(false);
  };
  checkShiftLongPress(Alignment.LEFT);
  checkShiftLongPress(Alignment.RIGHT);

  tester.scheduleTest('testShiftLongPressAsync', testDoneCallback);
}

/**
 * Asynchronously tests chording on the keyboard.
 * @param {function} testDoneCallBack The callback function to be called
 * on completion.
 */
function testShiftChordingAsync(testDoneCallback) {
  var tester = new ShiftKeyTester();

  var checkShiftChording = function(alignment) {
    tester.keyEvent(alignment, EventType.KEY_DOWN, false /* isLocked */,
        true /* chording */);

    tester.typeCharacter(true);

    tester.wait(1000, true);
    tester.keyEvent(alignment, EventType.KEY_UP, true);

    tester.typeCharacter(false);
  };
  checkShiftChording('left');
  checkShiftChording('right');

  tester.scheduleTest('testShiftChordingAsync', testDoneCallback);
}

/**
 * Asynchronously tests modifier keys on the keyboard.
 * @param {function} testDoneCallBack The callback function to be called
 * on completion.
 */
function testModifierKeysAsync(testDoneCallback) {
  var setupWork = function() {
    $('keyboard').layout = Layout.SYSTEM;
  };

  var onSystemQwertyLower = function() {
    assertEquals(Keyset.SYSTEM_LOWER, $('keyboard').activeKeysetId,
                 'Invalid keyset.');

    var ctrl = getModifierKey('Ctrl');
    var alt = getModifierKey('Alt');
    var mockEvent = {pointerId: 1};

    // Test 'ctrl' + 'a'.
    ctrl.down(mockEvent);
    ctrl.up(mockEvent);
    mockTypeCharacter('a', 0x41, Modifier.CONTROL);
    mockTypeCharacter('a', 0x41, Modifier.NONE);
    // Test 'ctrl' + 'alt' + 'a'.
    ctrl.down(mockEvent);
    ctrl.up(mockEvent);
    alt.down(mockEvent);
    alt.up(mockEvent);
    mockTypeCharacter('a', 0x41, Modifier.CONTROL | Modifier.ALT);
    mockTypeCharacter('a', 0x41, Modifier.NONE);
    // Test chording control.
    ctrl.down(mockEvent);
    mockTypeCharacter('a', 0x41, Modifier.CONTROL);
    mockTypeCharacter('a', 0x41, Modifier.CONTROL);
    ctrl.up(mockEvent);
    mockTypeCharacter('a', 0x41, Modifier.NONE);
    $('keyboard').layout = 'qwerty';
  };
  onSystemQwertyLower.waitCondition = {
    state: 'keysetChanged',
    value: Keyset.SYSTEM_LOWER
  };

  var onReset = function() {
    assertEquals(Keyset.DEFAULT_LOWER, $('keyboard').activeKeysetId,
                 'Invalid keyset.');
  };
  onReset.waitCondition = {
    state: 'keysetChanged',
    value: Keyset.DEFAULT_LOWER
  };

  onKeyboardReady('testModifierKeysAsync', setupWork, testDoneCallback,
      [onSystemQwertyLower, onReset]);
}

/**
 * Tests that pressing the hide keyboard key calls the appropriate hide keyboard
 * API. The test is run asynchronously since the keyboard loads keysets
 * dynamically.
 */
function testHideKeyboard(testDoneCallback) {
  var runTest = function() {
    var hideKey = $('keyboard').querySelector('kb-hide-keyboard-key');
    assertTrue(!!hideKey, 'Unable to find key');

    chrome.virtualKeyboardPrivate.hideKeyboard.addExpectation();

    hideKey.down();
    hideKey.up();
  };
  onKeyboardReady('testHideKeyboard', runTest, testDoneCallback);
}
