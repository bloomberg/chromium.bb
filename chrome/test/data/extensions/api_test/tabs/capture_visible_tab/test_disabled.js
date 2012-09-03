// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.tabs.captureVisibleTab(), screenshot disabling policy.
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureVisibleDisabled

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var assertFalse = chrome.test.assertFalse;

var kWindowRect = {
  'width': 400,
  'height': 400
};

var fail_url = "file:///nosuch.html";

chrome.test.runTests([
  function captureVisibleDisabled() {
    createWindow([fail_url], kWindowRect, pass(function(winId, tabIds) {
      waitForAllTabs(pass(function() {
        chrome.tabs.getSelected(winId, pass(function(tab) {
          assertEq('complete', tab.status);
          chrome.tabs.captureVisibleTab(winId, fail(
              'Taking screenshots has been disabled'));
        }));
      }));
    }));
  },

  function captureVisibleDisabledInNullWindow() {
    chrome.tabs.captureVisibleTab(null, fail(
        'Taking screenshots has been disabled'));
  },

  function captureVisibleDisabledInCurrentWindow() {
    chrome.tabs.captureVisibleTab(chrome.windows.WINDOW_ID_CURRENT,
                                  fail('Taking screenshots has been disabled'));
  }
]);
