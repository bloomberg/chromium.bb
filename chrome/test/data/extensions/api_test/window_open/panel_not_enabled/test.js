// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Verify that a popup results when disable-panels flag is set.
chrome.test.runTests([
  function openPanel() {
    chrome.windows.create(
        { 'url': 'about:blank', 'type': 'panel' },
        chrome.test.callbackPass(function(win) {
            chrome.test.assertEq('popup', win.type);
            chrome.test.assertEq(false, win.alwaysOnTop);
        }));
  }
]);
