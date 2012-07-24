// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function openPanel() {
    chrome.test.listenOnce(chrome.windows.onCreated, function(window) {
      chrome.test.assertTrue(window.width > 0);
      chrome.test.assertTrue(window.height > 0);
      chrome.test.assertEq("panel", window.type);
      chrome.test.assertTrue(!window.incognito);
    });
    chrome.windows.create(
        { 'url': 'about:blank', 'type': 'panel' },
        chrome.test.callbackPass(function(win) {
            chrome.test.assertEq('panel', win.type);
            chrome.test.assertEq(true, win.alwaysOnTop);
        }));
  }
]);
