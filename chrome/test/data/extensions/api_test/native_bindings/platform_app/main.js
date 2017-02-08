// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.assertTrue(!!chrome.app, 'app');
chrome.test.assertTrue(!!chrome.app.window, 'app.window');

var currentWindow = chrome.app.window.current();
// Current window is pretty funny and has a ton of custom JS bindings, also
// utilizing an internal API (currentWindowInternal). Test a bunch of stuff.
chrome.test.assertTrue(!!currentWindow, 'currentWindow');
// An instance property.
chrome.test.assertTrue(!!currentWindow.innerBounds, 'innerBounds');
// A method from the internal API.
chrome.test.assertTrue(!!currentWindow.drawAttention, 'drawAttention');
// A method on the prototype.
chrome.test.assertTrue(!!currentWindow.isFullscreen, 'isFullscreen');
// A property on the prototype.
chrome.test.assertTrue(!!currentWindow.contentWindow, 'contentWindow');

chrome.test.succeed();
