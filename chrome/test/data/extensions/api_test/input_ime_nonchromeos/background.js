// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testNormalCreateWindow() {
    var options = { windowType: 'normal' };
    chrome.input.ime.createWindow(options, function(win) {
      chrome.test.assertTrue(!chrome.runtime.lastError);
      chrome.test.assertTrue(!!win);
      chrome.test.assertTrue(win instanceof Window);
      chrome.test.assertFalse(win.document.webkitHidden);
      chrome.test.succeed();
    });
  },
  function testFollowCursorCreateWindow() {
    var options = { windowType: 'followCursor' };
    chrome.input.ime.createWindow(options, function(win) {
      chrome.test.assertTrue(!chrome.runtime.lastError);
      chrome.test.assertTrue(!!win);
      chrome.test.assertFalse(win.document.webkitHidden);
      chrome.test.succeed();
    });
  }
]);
