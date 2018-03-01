// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.tabs.captureVisibleTab(), when current window is null
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureNullWindow

var fail = chrome.test.callbackFail;

chrome.test.runTests([function captureNullWindow() {
  // Create a new window so we don't close the only active window.
  chrome.windows.create(function(newWindow) {
    chrome.windows.remove(newWindow.id);
    chrome.tabs.captureVisibleTab(
        newWindow.id, fail('Failed to capture tab: view is invisible'));
  });
}]);
