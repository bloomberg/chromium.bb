/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Mocks using the longpress candidate window to enter an alternate character.
 * @param {string} label The label on the key.
 * @param {string} candidateLabel The alternate character being typed.
 * @param {number} keyCode The keyCode for the alternate character, which may
 *     be zero for characters not found on a QWERTY keyboard.
 * @param {number} modifiers Indicates the state of the shift, control and alt
 *     key when entering the character.
 * @param {Object.<string, boolean>=} opt_variant Optional test variant.
 */
function mockLongPressType(label,
                           candidateLabel,
                           keyCode,
                           modifiers,
                           opt_variant) {
  // Verify that candidates popup window is initally hidden.
  var keyset = keyboard.querySelector('#' + keyboard.activeKeysetId);
  assertTrue(!!keyset, 'Unable to find active keyset.');
  var mockEvent = { pointerId:1 };
  var candidatesPopup = keyset.querySelector('kb-altkey-container');
  assertTrue(!!candidatesPopup, 'Unable to find altkey container.');
  assertTrue(candidatesPopup.hidden,
             'Candidate popup should be hidden initially.');

  // Show candidate window of alternate keys on a long press.
  var key = findKey(label);
  var altKey = null;
  assertTrue(!!key, 'Unable to find key labelled "' + label + '".');
  key.down(mockEvent);
  mockTimer.tick(1000);
  if (opt_variant && opt_variant.noCandidates) {
    assertTrue(candidatesPopup.hidden, 'Candidate popup should remain hidden.');
  } else {
    assertFalse(candidatesPopup.hidden, 'Candidate popup should be visible.');

    // Verify that the popup window contains the candidate key.
    var candidates = candidatesPopup.querySelectorAll('kb-altkey');
    for (var i = 0; i < candidates.length; i++) {
       if (candidates[i].innerText == candidateLabel) {
         altKey = candidates[i];
         break;
       }
    }
    assertTrue(!!altKey, 'Unable to find key in candidate list.');
  }

  var abortSelection = opt_variant && opt_variant.abortSelection;
  if (!abortSelection) {
    // Verify that the candidate key is typed on release of the longpress.
    var send = chrome.virtualKeyboardPrivate.sendKeyEvent;
    var unicodeValue = candidateLabel.charCodeAt(0);
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
  }
  if (altKey) {
    // A keyout event should be dispatched before a keyover event.
    key.out(mockEvent);
    altKey.over({pointerId: 1, relatedTarget: altKey});
    if (abortSelection)
      altKey.out({pointerId: 1, relatedTarget: altKey});
    else
      altKey.up(mockEvent);

    // Verify that the candidate list is hidden on a pointer up event.
    candidatesPopup.up(mockEvent);
    assertTrue(candidatesPopup.hidden,
               'Candidate popup should be hidden after inserting a character.');
  } else {
    key.up(mockEvent);
  }
}

/**
 * Tests that typing characters on the default lowercase keyboard triggers the
 * correct sequence of events. The test is run asynchronously since the
 * keyboard loads keysets dynamically.
 */
function testLowercaseKeysetAsync(testDoneCallback) {
  var runTest = function() {
    // Keyboard defaults to lowercase.
    mockTypeCharacter('a', 0x41, Modifier.NONE);
    mockTypeCharacter('s', 0x53, Modifier.NONE);
    mockTypeCharacter('.', 0xBE, Modifier.NONE);
    mockTypeCharacter('\b', 0x08, Modifier.NONE, 0x08);
    mockTypeCharacter('\t', 0x09, Modifier.NONE, 0x09);
    mockTypeCharacter('\n', 0x0D, Modifier.NONE, 0x0A);
    mockTypeCharacter(' ', 0x20, Modifier.NONE);
  };
  onKeyboardReady('testLowercaseKeysetAsync', runTest, testDoneCallback);
}

/**
 * Tests long press on a key that has alternate sugestions. For example,
 * longpressing the 'a' key displays 'a acute' 'a grave', etc. Longpressing
 * characters on the top row of the keyboard displays numbers as alternatives.
 */
function testLongPressTypeAccentedCharacterAsync(testDoneCallback) {
  var runTest = function() {
    // Test popup for letters with candidate lists that are derived from a
    // single source (hintText or accents).
    // Type lowercase A grave
    mockLongPressType('a', '\u00E0', 0, Modifier.NONE);
    // Type the digit '1' (hintText on 'q' key).
    mockLongPressType('q', '1', 0x31, Modifier.NONE);

    // Test popup for letter that has a candidate list combining hintText and
    // accented letters.
    // Type lowercase E acute.
    mockLongPressType('e', '\u00E9', 0, Modifier.NONE);
    // Type the digit '3' (hintText on the 'e' key).
    mockLongPressType('e', '3', 0x33, Modifier.NONE);

    // Mock longpress typing a character that does not have alternate
    // candidates.
    mockLongPressType('z', 'z', 0x5A, Modifier.NONE, {noCandidates: true});

    // Mock aborting a longpress selection.
    mockLongPressType('e', '3', 0x33, Modifier.NONE, {abortSelection: true});
  };
  onKeyboardReady('testLongPressTypeAccentedCharacterAsync',
                  runTest,
                  testDoneCallback);
}

/**
 * When typing quickly, one can often press a second key before releasing the
 * first. This test confirms that both keys are typed in the correct order.
 */
function testAutoReleasePreviousKey(testDoneCallback) {
  var runTest = function() {
    var key = findKey('a');
    assertTrue(!!key, 'Unable to find key labelled "a".');
    var unicodeValue = 'a'.charCodeAt(0);
    var send = chrome.virtualKeyboardPrivate.sendKeyEvent;
    send.addExpectation({
      type: 'keydown',
      charValue: unicodeValue,
      keyCode: 0x41,
      modifiers: Modifier.NONE
    });
    send.addExpectation({
      type: 'keyup',
      charValue: unicodeValue,
      keyCode: 0x41,
      modifiers: Modifier.NONE
    });
    var mockEvent = { pointerId:2 };
    key.down(mockEvent);
    mockTypeCharacter('s', 0x53, Modifier.NONE);
  };
  onKeyboardReady('testAutoReleasePreviousKey', runTest, testDoneCallback);
}

/**
 * When touch typing, one can often press a key and move slightly out of the key
 * area before releasing the key. This test confirms that the key is not
 * dropped.
 */
function testFingerOutType(testDoneCallback) {
  var runTest = function() {
    var key = findKey('a');
    assertTrue(!!key, 'Unable to find key labelled "a".');
    var unicodeValue = 'a'.charCodeAt(0);
    var send = chrome.virtualKeyboardPrivate.sendKeyEvent;

    // Test finger moves out of typed key slightly before release. The key
    // should not be dropped.
    send.addExpectation({
      type: 'keydown',
      charValue: unicodeValue,
      keyCode: 0x41,
      modifiers: Modifier.NONE
    });
    send.addExpectation({
      type: 'keyup',
      charValue: unicodeValue,
      keyCode: 0x41,
      modifiers: Modifier.NONE
    });
    var mockEvent = { pointerId:2 };
    key.down(mockEvent);
    key.out(mockEvent);
    // Mocks finger releases after moved out of the 'a' key.
    keyboard.up(mockEvent);

    // Test a second finger types on a different key before first finger
    // releases (yet moves out of the typed key). The first typed key should not
    // be dropped.
    send.addExpectation({
      type: 'keydown',
      charValue: unicodeValue,
      keyCode: 0x41,
      modifiers: Modifier.NONE
    });
    send.addExpectation({
      type: 'keyup',
      charValue: unicodeValue,
      keyCode: 0x41,
      modifiers: Modifier.NONE
    });
    key.down(mockEvent);
    key.out(mockEvent);
    mockTypeCharacter('s', 0x53, Modifier.NONE);
  };
  onKeyboardReady('testFingerOutType', runTest, testDoneCallback);
}
