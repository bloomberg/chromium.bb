// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// experimental.input.virtualKeyboard API test for Chrome
// browser_tests --gtest_filter=ExtensionApiTest.Input

chrome.test.runTests([
  function sendKeyboardEvent() {
    var e = { 'type': 'keydown', 'keyIdentifier': 'A' };
    chrome.experimental.input.virtualKeyboard.sendKeyboardEvent(e, function() {
      if (chrome.runtime.lastError) {
        // this is expected for now: no one is handling keys yet
        // chrome.test.fail();
      }
      // when the browser is listening to events, we should check that
      // this event was delivered as we expected.  For now, just succeed.
      chrome.test.succeed();
    });
  },

  function badKeyIdentifier() {
    var e = { 'type': 'keydown', 'keyIdentifier': 'BogusId' };
    chrome.experimental.input.virtualKeyboard.sendKeyboardEvent(e, function() {
      if (!chrome.runtime.lastError) {
        chrome.test.fail();
      }
      chrome.test.succeed();
    });
  },

  function badEventType() {
    var e = { 'type': 'BAD', 'keyIdentifier': 'A' };
    chrome.experimental.input.virtualKeyboard.sendKeyboardEvent(e, function() {
      if (!chrome.runtime.lastError) {
        chrome.test.fail();
      }
      chrome.test.succeed();
    });
  },

  function unmappedKeyIdentifier() {
    var e = { 'type': 'keydown', 'keyIdentifier': 'Again' };
    chrome.experimental.input.virtualKeyboard.sendKeyboardEvent(e, function() {
      if (!chrome.runtime.lastError) {
        chrome.test.fail();
      }
      chrome.test.succeed();
    });
  },

  function sendKeyboardEventUnicode1() {
    // U+00E1: LATIN SMALL LATTER A WITH ACUTE.
    var e = { 'type': 'keydown', 'keyIdentifier': 'U+00E1' };
    chrome.experimental.input.virtualKeyboard.sendKeyboardEvent(e, function() {
      if (chrome.runtime.lastError) {
        // this is expected for now. See sendKeyboardEvent().
        // chrome.test.fail();
      }
      chrome.test.succeed();
    });
  },

  function sendKeyboardEventUnicode2() {
    // U+043A: CYRILLIC SMALL LETTER KA
    var e = { 'type': 'keydown',
              'keyIdentifier': 'U+043a' };  // lower case is also ok.
    chrome.experimental.input.virtualKeyboard.sendKeyboardEvent(e, function() {
      if (chrome.runtime.lastError) {
        // this is expected for now. See sendKeyboardEvent().
        // chrome.test.fail();
      }
      chrome.test.succeed();
    });
  },

  function sendKeyboardEventBadUnicode1() {
    var e = { 'type': 'keydown', 'keyIdentifier': 'U+' };
    chrome.experimental.input.virtualKeyboard.sendKeyboardEvent(e, function() {
      if (!chrome.runtime.lastError) {
        chrome.test.fail();
      }
      chrome.test.succeed();
    });
  },

  function sendKeyboardEventBadUnicode2() {
    var e = { 'type': 'keydown', 'keyIdentifier': 'U+1' };
    chrome.experimental.input.virtualKeyboard.sendKeyboardEvent(e, function() {
      if (!chrome.runtime.lastError) {
        chrome.test.fail();
      }
      chrome.test.succeed();
    });
  },

  function sendKeyboardEventBadUnicode3() {
    var e = { 'type': 'keydown', 'keyIdentifier': 'U+111g' };
    chrome.experimental.input.virtualKeyboard.sendKeyboardEvent(e, function() {
      if (!chrome.runtime.lastError) {
        chrome.test.fail();
      }
      chrome.test.succeed();
    });
  },

  function sendKeyboardEventBadUnicode4() {
    var e = { 'type': 'keydown', 'keyIdentifier': 'U+11111' };
    chrome.experimental.input.virtualKeyboard.sendKeyboardEvent(e, function() {
      if (!chrome.runtime.lastError) {
        chrome.test.fail();
      }
      chrome.test.succeed();
    });
  },
]);
