// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testActivate() {
    var focused = false;
    var activated = false;
    chrome.input.ime.onFocus.addListener(function(context) {
      if (context.type == 'none') {
        chrome.test.fail();
        return;
      }
      focused = true;
      if (activated)
        chrome.test.succeed();
    });
    chrome.input.ime.activate(function() {
      if (chrome.runtime.lastError) {
        chrome.test.fail();
        return;
      }
      activated = true;
      if (focused)
        chrome.test.succeed();
    });
  },
  function testNormalCreateWindow() {
    var options = { windowType: 'normal' };
    chrome.input.ime.createWindow(options, function(win) {
      chrome.test.assertNoLastError()
      chrome.test.assertTrue(!!win);
      chrome.test.assertTrue(win instanceof Window);
      chrome.test.assertFalse(win.document.webkitHidden);
      chrome.test.succeed();
    });
  },
  function testFollowCursorCreateWindow() {
    var options = { windowType: 'followCursor' };
    chrome.input.ime.createWindow(options, function(win) {
      chrome.test.assertNoLastError()
      chrome.test.assertTrue(!!win);
      chrome.test.assertFalse(win.document.webkitHidden);
      chrome.test.succeed();
    });
  }
]);
