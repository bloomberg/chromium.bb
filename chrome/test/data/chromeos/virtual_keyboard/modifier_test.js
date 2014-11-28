/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Test that the Control modifier key is sticky.
 */
function testControlKeyStickyAsync(testDoneCallback) {
  var testCallback = function() {
    mockTap(findKeyById('ControlLeft'));
    var send = chrome.virtualKeyboardPrivate.sendKeyEvent;
    send.addExpectation({
      type: 'keydown',
      charValue: 0,
      keyCode: 65,
      modifiers: Modifier.CONTROL
    });
    send.addExpectation({
      type: 'keyup',
      charValue: 0,
      keyCode: 65,
      modifiers: Modifier.CONTROL
    });
    mockTap(findKey('a'));

    // Ensure that the control key is no longer sticking. i.e. Ensure that
    // typing 'a' on its own results in only 'a'.
    mockTypeCharacter('a', 0x41, Modifier.NONE);

    testDoneCallback();
  };
  var config = {
    keyset: 'us',
    languageCode: 'en',
    passwordLayout: 'us',
    name: 'English'
  };
  onKeyboardReady(testCallback, config);
}
