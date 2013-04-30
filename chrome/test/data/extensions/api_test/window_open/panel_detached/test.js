// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createCallback(win) {
  chrome.test.assertEq('panel', win.type);
  // Unlike docked panels, detached is not alwaysOnTop.
  chrome.test.assertEq(false, win.alwaysOnTop);
}

chrome.test.runTests([
  // No origin nor size is specified.
  function openDetachedPanel() {
    chrome.test.listenOnce(chrome.windows.onCreated, function(window) {
      chrome.test.assertEq("panel", window.type);
      chrome.test.assertTrue(!window.incognito);
      chrome.test.assertTrue(window.width > 0);
      chrome.test.assertTrue(window.height > 0);
    });
    chrome.windows.create(
        { 'url': 'about:blank',
          'type': 'detached_panel' },
        chrome.test.callbackPass(createCallback));
  },

  // Verify supplied size is obeyed even if no origin is specified.
  function openDetachedPanelWithSize() {
    chrome.test.listenOnce(chrome.windows.onCreated, function(window) {
      chrome.test.assertEq("panel", window.type);
      chrome.test.assertTrue(!window.incognito);
      chrome.test.assertEq(250, window.width);
      chrome.test.assertEq(300, window.height);
    });
    chrome.windows.create(
        { 'url': 'about:blank',
          'type': 'detached_panel', 'width': 250, 'height': 300 },
        chrome.test.callbackPass(createCallback));
  },

  // Verify supplied origin is obeyed even if no size is specified.
  function openDetachedPanelWithOrigin() {
    chrome.test.listenOnce(chrome.windows.onCreated, function(window) {
      chrome.test.assertEq("panel", window.type);
      chrome.test.assertTrue(!window.incognito);
      chrome.test.assertEq(42, window.top);
      chrome.test.assertEq(24, window.left);
      chrome.test.assertTrue(window.width > 0);
      chrome.test.assertTrue(window.height > 0);
    });
    chrome.windows.create(
        { 'url': 'about:blank',
          'type': 'detached_panel', 'top': 42, 'left': 24 },
        chrome.test.callbackPass(createCallback));
  },

  // Verify supplied bounds are obeyed.
  function openDetachedPanelWithFullBounds() {
    chrome.test.listenOnce(chrome.windows.onCreated, function(window) {
      chrome.test.assertEq("panel", window.type);
      chrome.test.assertTrue(!window.incognito);
      chrome.test.assertEq(42, window.top);
      chrome.test.assertEq(24, window.left);
      chrome.test.assertEq(250, window.width);
      chrome.test.assertEq(300, window.height);
    });
    chrome.windows.create(
        { 'url': 'about:blank',
          'type': 'detached_panel', 'top': 42, 'left': 24,
          'width': 250, 'height': 300 },
        chrome.test.callbackPass(createCallback));
  }
]);
