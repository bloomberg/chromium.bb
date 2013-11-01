/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Retrieves the left or right shift keys. Shift keys come in pairs for upper
 * and lowercase keyboard layouts respectively.
 * @param {string} alignment Which of {left, right} shift keys to return.
 * @return {Object.<string: Object>} a map from keyset to the shift key in it.
 */
function getShiftKeys(alignment) {
  var layout = keyboard.layout;
  var keysets = ['lower', 'upper'];

  var result={};
  for(var keysetIndex in keysets) {
    var keysetId = keysets[keysetIndex];
    // The keyset DOM object which contains a shift key.
    var keyset = keyboard.querySelector('#' + layout + "-" + keysetId);
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
  var keyset = keyboard.querySelector('#system-qwerty-lower');
  assertTrue(!!keyset, "Cannot find keyset: " + keyset);
  var modifierKey = keyset.querySelector('kb-modifier-key[char="' +
      modifier + '"]');
  assertTrue(!!modifierKey, "Modifier key not found: " + modifierKey);
  return modifierKey;
}

/**
 * Tests chording with a single shift key.
 * @param {Object} lowerShift, a shift key in the lower key set.
 * @param {Object} uppserShift, the same shift key in the upper key set.
 */
function checkShiftChording(lowerShift, upperShift) {
  var lower = lowerShift.lowerCaseKeysetId;
  var upper = lowerShift.upperCaseKeysetId;
  // Check that we're testing from correct initial state.
  assertEquals(lower, keyboard.keyset, "Invalid initial keyset.");
  var mockEventOnLower = { pointerId:1, isPrimary:true, target:lowerShift };
  var mockEventOnUpper = { pointerId:1, isPrimary:true, target:upperShift };
  lowerShift.down(mockEventOnLower);
  assertEquals(upper, keyboard.keyset,
      "Unexpected keyset transition on shift key down.");
  // Some alphanumeric character
  mockTypeCharacter('A', 0x41, Modifier.SHIFT);
  assertEquals(upper, keyboard.keyset,
      "Did not remain in uppercase on key press while chording.");
  lowerShift.out(mockEventOnLower);
  assertEquals(upper, keyboard.keyset,
      "Did not remain in uppercase on a finger movement while chording.");
  mockTimer.tick(1000);
  upperShift.up(mockEventOnUpper);
  assertEquals(lower, keyboard.keyset,
      "Did not revert to lowercase after chording.");
}

/**
 * Tests a particular shift key for highlighting on tapping. The keyboard
 * should temporarily transition to uppercase, and after a non-control tap,
 * revert to lower case.
 * @param {Object} lowerShift The shift key object on the lower keyset.
 * @param {Object} upperShift The shift key object on the upper keyset.
 */
function checkShiftHighlight(lowerShift, upperShift) {
  assertTrue(!!keyboard.shift, "Shift key was not cached by keyboard");
  var unlocked = lowerShift.lowerCaseKeysetId;
  var locked = lowerShift.upperCaseKeysetId;
  assertEquals(unlocked, keyboard.keyset,
      "Invalid initial keyboard keyset.");
  // Crashes if we don't give it a pointerId.
  var mockEvent = { pointerId: 1};
  // Tap shift key.
  lowerShift.down(mockEvent);
  upperShift.up(mockEvent);
  // Tests that we are now in locked case.
  assertEquals(locked, keyboard.keyset,
      "Unexpected keyset transition after typing shift.");
  // Some alphanumeric character.
  mockTypeCharacter('A', 0x41, Modifier.SHIFT);
  // Check that we've reverted to lower case.
  assertEquals(unlocked, keyboard.keyset,
      "Did not revert to lower case after highlight.");
  // Check that we persist in lower case.
  mockTypeCharacter('a', 0x41, Modifier.NONE);
  assertEquals(unlocked, keyboard.keyset,
      "Did not stay in lower case after highlight.");
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
 * Tests that a particular shift key capitalizes on long press.
 * @param {Object} shift The shift key.
 */
function checkShiftLongPress(lowerShift, upperShift) {
  // Check that keyboard is in expected start state.
  assertTrue(!!keyboard.shift, "Shift key was not cached by keyboard");
  var unlocked = lowerShift.lowerCaseKeysetId;
  var locked = lowerShift.upperCaseKeysetId;
  assertEquals(unlocked, keyboard.keyset,
      "Invalid initial keyboard keyset.");
  // Mocks a pointer event.
  var mockEvent = { pointerId: 1};
  lowerShift.down(mockEvent);
  assertEquals(locked, keyboard.keyset,
      "Invalid transition on shift key down.");
  // Long press should now be active.
  mockTimer.tick(1000);
  // Type any caps character, make sure we remain in caps mode.
  upperShift.up(mockEvent);
  mockTypeCharacter('A', 0x41, Modifier.SHIFT);
  assertEquals(locked, keyboard.keyset,
      "Did not remain in locked case after shift long press.");
  // Revert to lower case.
  upperShift.down(mockEvent);
  assertEquals(unlocked, keyboard.keyset,
      "Did not revert to lower case on shift down.");
  lowerShift.up(mockEvent);
}

/**
 * Tests that a particular shift key capitalizes on double click.
 * @param {Object} lowerShift The shift key in the lower keyset.
 * @param {Object} upperShift The shift key in the upper keyset.
 */
function checkShiftDoubleClick(lowerShift, upperShift) {
  // Check that keyboard is in expected start state.
  assertTrue(!!keyboard.shift, "Shift key was not cached by keyboard");
  var unlocked = lowerShift.lowerCaseKeysetId;
  var locked = lowerShift.upperCaseKeysetId;
  assertEquals(unlocked, keyboard.keyset,
      "Invalid initial keyboard keyset.");
  // Mocks a pointer event.
  var mockEvent = {pointerId: 1};
  lowerShift.down(mockEvent);
  upperShift.up(mockEvent);
  // Need to also mock a keyboard pointer up event.
  keyboard.up(mockEvent);
  upperShift.down(mockEvent);
  upperShift.up(mockEvent);
  keyboard.up(mockEvent);
  // Check that we're capslocked.
  assertEquals(locked, keyboard.keyset,
      "Did not lock on double click.");
  mockTypeCharacter('A', 0x41, Modifier.SHIFT);
  assertEquals(locked, keyboard.keyset,
      "Did not remain in locked case after typing another key.");
  // Reverts to lower case.
  upperShift.down(mockEvent);
  assertEquals(unlocked, keyboard.keyset,
      "Did not revert to lower case on shift down.");
  lowerShift.up(mockEvent);
}

/**
 * Asynchronously tests highlighting of the left and right shift keys.
 * @param {function} testDoneCallBack The function to be called
 * on completion.
 */
function testShiftHighlightAsync(testDoneCallback) {
  var runTest = function() {
    var alignments = ['left', 'right'];
    for (var i in alignments) {
      var alignment = alignments[i];
      var shifts = getShiftKeys(alignment);
      checkShiftHighlight(shifts['lower'], shifts['upper']);
   }
  };
  onKeyboardReady('testShiftKeyHighlightAsync', runTest, testDoneCallback);
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
    var runTest = function() {
    var alignments = ['left', 'right'];
    for (var i in alignments) {
      var alignment = alignments[i];
      var shifts = getShiftKeys(alignment);
      checkShiftDoubleClick(shifts['lower'], shifts['upper']);
   }
  };
  onKeyboardReady('testShiftDoubleClickAsync', runTest, testDoneCallback);
}

/**
 * Asynchronously tests capitalization on long press of the left and
 * right shift keys.
 * @param {function} testDoneCallBack The callback function to be called
 * on completion.
 */
function testShiftLongPressAsync(testDoneCallback) {
  var runTest = function() {
    var alignments = ['left', 'right'];
    for (var i in alignments) {
      var alignment = alignments[i];
      var shifts = getShiftKeys(alignment);
      checkShiftLongPress(shifts['lower'], shifts['upper']);
   }
  };
  onKeyboardReady('testShiftLongPressAsync', runTest, testDoneCallback);
}

/**
 * Asynchronously tests chording on the keyboard.
 * @param {function} testDoneCallBack The callback function to be called
 * on completion.
 */
function testShiftChordingAsync(testDoneCallback) {
  var runTest = function() {
    var left = getShiftKeys('left');
    var right = getShiftKeys('right');
    checkShiftChording(left['lower'], left['upper']);
    checkShiftChording(right['lower'], right['upper']);
   }
  onKeyboardReady('testShiftChordingAsync', runTest, testDoneCallback);
}

/**
 * Asynchronously tests modifier keys on the keyboard.
 * @param {function} testDoneCallBack The callback function to be called
 * on completion.
 */
function testModifierKeysAsync(testDoneCallback) {
  var setupWork = function() {
    keyboard.layout = 'system-qwerty';
  }
  var subtasks = [];
  subtasks.push( function() {
    assertEquals("system-qwerty-lower",
        keyboard.activeKeysetId, "Unexpected layout transition.");
    var ctrl = getModifierKey('Ctrl');
    var alt = getModifierKey('Alt');
    var mockEvent = {pointerId: 1};
    // Test 'ctrl' + 'a'.
    ctrl.down(mockEvent);
    ctrl.up(mockEvent);
    mockTypeCharacter('a', 0x41, 4);
    mockTypeCharacter('a', 0x41, 0);
    // Test 'ctrl' + 'alt' + 'a'.
    ctrl.down(mockEvent);
    ctrl.up(mockEvent);
    alt.down(mockEvent);
    alt.up(mockEvent);
    mockTypeCharacter('a', 0x41, 12);
    mockTypeCharacter('a', 0x41, 0);
    // Test chording control.
    ctrl.down(mockEvent);
    mockTypeCharacter('a', 0x41, 4);
    mockTypeCharacter('a', 0x41, 4);
    ctrl.up(mockEvent);
    mockTypeCharacter('a', 0x41, 0);
    keyboard.layout = 'qwerty';
  });
  // Make sure we end in a stable state.
  subtasks.push(function() {
    assertEquals("qwerty-lower", keyboard.activeKeysetId,
        "Unexpected final keyset.");
  });
  onKeyboardReady('testModifierKeysAsync', setupWork, testDoneCallback,
      subtasks);
}

/**
 * Tests that pressing the hide keyboard key calls the appropriate hide keyboard
 * API. The test is run asynchronously since the keyboard loads keysets
 * dynamically.
 */
function testHideKeyboard(testDoneCallback) {
  var runTest = function() {
    var hideKey = keyboard.querySelector('kb-hide-keyboard-key');
    assertTrue(!!hideKey, 'Unable to find key');

    chrome.virtualKeyboardPrivate.hideKeyboard.addExpectation();

    hideKey.down();
    hideKey.up();
  };
  onKeyboardReady('testHideKeyboard', runTest, testDoneCallback);
}
