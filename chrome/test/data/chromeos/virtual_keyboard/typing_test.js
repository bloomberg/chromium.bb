/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Mock typing of basic keys. Each keystroke should trigger a pair of
 * API calls to send viritual key events.
 * @param {string} label The character being typed.
 * @param {number} keyCode The legacy key code for the character.
 * @param {boolean} shiftModifier Indicates if the shift key is being
 *     virtually pressed.
 * @param {number=} opt_unicode Optional unicode value for the character. Only
 *     required if it cannot be directly calculated from the label.
 */
function mockTypeCharacter(label, keyCode, shiftModifier, opt_unicode) {
  var key = findKey(label);
  assertTrue(!!key, 'Unable to find key labelled "' + label + '".');
  var unicodeValue = opt_unicode | label.charCodeAt(0);
  var send = chrome.virtualKeyboardPrivate.sendKeyEvent;
  send.addExpectation({
    type: 'keydown',
    charValue: unicodeValue,
    keyCode: keyCode,
    shiftKey: shiftModifier
  });
  send.addExpectation({
    type: 'keyup',
    charValue: unicodeValue,
    keyCode: keyCode,
    shiftKey: shiftModifier
  });
  // Fake typing the key.
  key.down();
  key.up();
}

/**
 * Tests that typing characters on the default lowercase keyboard triggers the
 * correct sequence of events.  The test is run asynchronously since the
 * keyboard loads keysets dynamically.
 */
function testLowercaseKeysetAsync(testDoneCallback) {
  var runTest = function() {
    // Keyboard defaults to lowercase.
    mockTypeCharacter('a', 0x41, false);
    mockTypeCharacter('s', 0x53, false);
    mockTypeCharacter('.', 0xBE, false);
    mockTypeCharacter('\b', 0x08, false, 0x08);
    mockTypeCharacter('\t', 0x09, false, 0x09);
    mockTypeCharacter('\n', 0x0D, false, 0x0A);
    mockTypeCharacter(' ', 0x20, false);
  };
  onKeyboardReady('testLowercaseKeysetAsync', runTest, testDoneCallback);
}
