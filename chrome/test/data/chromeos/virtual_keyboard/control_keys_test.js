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
   * Extends the subtask scheduler.
   */
  __proto__: SubtaskScheduler.prototype,

  /**
   * Adds a subtask for mocking a single key event on a shift key.
   * @param {string} alignment Indicates if triggering on the left or right
   *     shift key.
   * @param {string} keyOp Name of the event.
   * @param {string} keysetId Expected keyset at the start of the subtask.
   * @param {boolean=} opt_chording Optional parameter to indicate that the key
   *     event is for a chording operation.
   */
  keyEvent: function(alignment, keyOp, keysetId, opt_chording) {
    var self = this;
    var fn = function() {
      Debug('Mocking shift key event: alignment = ' + alignment + ', keyOp = ' +
          keyOp + ', keysetId = ' + keysetId + ', chording = ' +
          (opt_chording ? true : false));
      self.alignment = alignment;
      self.verifyKeyset(keysetId,
          'Unexpected keyset before shift key event.');
      var keys = $('keyboard').activeKeyset.querySelectorAll('kb-shift-key');
      shiftKey = keys[this.alignment == 'left' ? 0 : 1];
      var mockKeyEvent = {pointerId: 2, isPrimary:true, target: shiftKey};
      shiftKey[keyOp](mockKeyEvent);
      // An 'up" event needs to fire a pointer up on the keyboard in order to
      // handle click counting.
      if (keyOp == 'up')
        $('keyboard').up(mockKeyEvent);
      if (opt_chording)
        shiftKey.out(mockKeyEvent);
    };
    this.addWaitCondition(fn, keysetId);
    this.addSubtask(fn);
  },

  /**
   * Adds a subtask for mocking the typing of a lowercase or uppercase 'A'.
   * @param {boolean} isUppercase Indicates the case of the character to type.
   */
  typeCharacterA: function(keysetId) {
    var self = this;
    var fn = function() {
      var isUppercase = keysetId == Keyset.UPPER;
      Debug('Mock typing ' + (isUppercase ? 'A' : 'a'));
      self.verifyKeyset(keysetId,
          'Unexpected keyset for typed character.');
      mockTypeCharacter(isUppercase ? 'A' : 'a', 0x41,
                        isUppercase ? Modifier.SHIFT : Modifier.NONE);
    };
    this.addWaitCondition(fn, keysetId);
    this.addSubtask(fn);
  }
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
    tester.keyEvent(alignment, EventType.KEY_DOWN, Keyset.LOWER);
    tester.keyEvent(alignment, EventType.KEY_UP, Keyset.UPPER);
    tester.typeCharacterA(Keyset.UPPER);
    tester.typeCharacterA(Keyset.LOWER);
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
    tester.keyEvent(alignment, EventType.KEY_DOWN, Keyset.LOWER);
    tester.keyEvent(alignment, EventType.KEY_UP, Keyset.UPPER);
    tester.keyEvent(alignment, EventType.KEY_DOWN, Keyset.UPPER);
    tester.keyEvent(alignment, EventType.KEY_UP, Keyset.UPPER);

    tester.typeCharacterA(Keyset.UPPER);
    tester.typeCharacterA(Keyset.UPPER);

    tester.keyEvent(alignment, EventType.KEY_DOWN, Keyset.UPPER);
    tester.keyEvent(alignment, EventType.KEY_UP, Keyset.LOWER);

    tester.typeCharacterA(Keyset.LOWER);
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
    tester.keyEvent(alignment, EventType.KEY_DOWN, Keyset.LOWER);
    tester.wait(1000, Keyset.UPPER);
    tester.keyEvent(alignment, EventType.KEY_UP, Keyset.UPPER);

    tester.typeCharacterA(Keyset.UPPER);
    tester.typeCharacterA(Keyset.UPPER);

    tester.keyEvent(alignment, EventType.KEY_DOWN, Keyset.UPPER);
    tester.keyEvent(alignment, EventType.KEY_UP, Keyset.LOWER);

    tester.typeCharacterA(Keyset.LOWER);
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
    tester.keyEvent(alignment, EventType.KEY_DOWN, Keyset.LOWER,
        true /* chording */);

    tester.typeCharacterA(Keyset.UPPER);

    tester.wait(1000, Keyset.UPPER);
    tester.keyEvent(alignment, EventType.KEY_UP, Keyset.UPPER);

    tester.typeCharacterA(Keyset.LOWER);
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
