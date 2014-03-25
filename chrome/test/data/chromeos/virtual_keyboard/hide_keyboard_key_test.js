/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Tests that tapping the hide keyboard button hides the keyboard.
 * @param {Function} testDoneCallback The callback function on completion.
 */
function testHideKeyboard(testDoneCallback) {
  var self = this;
  var mockEvent = {pointerId: 1};
  mockEvent.stopPropagation = function() {};
  var fn = function() {
    Debug('Mock tap on hide keyboard key.');
    var hideKey =
        $('keyboard').activeKeyset.querySelector('kb-hide-keyboard-key');
    assertTrue(!!hideKey, 'Unable to find hide keyboard key.');

    hideKey.down(mockEvent);
    // Expect hideKeyboard to be called.
    chrome.virtualKeyboardPrivate.hideKeyboard.addExpectation();
    // hideKeyboard in api_adapter also unlocks the keyboard.
    chrome.virtualKeyboardPrivate.lockKeyboard.addExpectation(false);
    hideKey.up(mockEvent);
  };
  onKeyboardReady('testHideKeyboard', fn, testDoneCallback);
}
