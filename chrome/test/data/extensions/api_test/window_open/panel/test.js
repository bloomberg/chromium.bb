// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var panelWindowId = 0;

// This function is called by the panel during the test run.
function panelCallback() {
  // We have now added a panel so the total counts is 2 (browser + panel).
  chrome.test.assertEq(2, chrome.extension.getViews().length);
  // Verify that we're able to get the view of the panel by its window id.
  chrome.test.assertEq(1,
      chrome.extension.getViews({"windowId": panelWindowId}).length);
  chrome.test.notifyPass();
}

chrome.test.runTests([
  function openPanel() {
    chrome.test.listenOnce(chrome.windows.onCreated, function(window) {
      chrome.test.assertTrue(window.width > 0);
      chrome.test.assertTrue(window.height > 0);
      chrome.test.assertEq("panel", window.type);
      chrome.test.assertTrue(!window.incognito);
    });
    chrome.windows.create(
        { 'url': chrome.extension.getURL('panel.html'), 'type': 'panel' },
        function(win) {
            chrome.test.assertEq('panel', win.type);
            chrome.test.assertEq(true, win.alwaysOnTop);
            panelWindowId = win.id;
            // The panel will call back to us through panelCallback (above).
        });
  }
]);
