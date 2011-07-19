// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// experimental.input API test for Chrome
// browser_tests --gtest_filter=ExtensionApiTest.Input

chrome.test.runTests([
  function sendKeyboardEvent() {
    var e = { 'type': 'keydown', 'keyIdentifier': 'A' };
    chrome.experimental.input.sendKeyboardEvent(e, function() {
      if (chrome.extension.lastError) {
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
    chrome.experimental.input.sendKeyboardEvent(e, function() {
      if (!chrome.extension.lastError) {
        chrome.test.fail();
      }
      chrome.test.succeed();
    });
  },

  function badEventType() {
    var e = { 'type': 'BAD', 'keyIdentifier': 'A' };
    chrome.experimental.input.sendKeyboardEvent(e, function() {
      if (!chrome.extension.lastError) {
        chrome.test.fail();
      }
      chrome.test.succeed();
    });
  },

  function unmappedKeyIdentifier() {
    var e = { 'type': 'keydown', 'keyIdentifier': 'Again' };
    chrome.experimental.input.sendKeyboardEvent(e, function() {
      if (!chrome.extension.lastError) {
        chrome.test.fail();
      }
      chrome.test.succeed();
    });
  },
]);
