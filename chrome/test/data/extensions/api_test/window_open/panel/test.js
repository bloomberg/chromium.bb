// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function openPanel() {
    chrome.windows.create(
        { 'url': 'about:blank', 'type': 'panel' },
        chrome.test.callbackPass(function(win) {
            chrome.test.assertEq('panel', win.type);
        }));
  }
]);
