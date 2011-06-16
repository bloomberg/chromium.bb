// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.tabs.captureVisibleTab(), capturing JPEG images.
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureVisibleNoFile

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
  // Check that test infrastructure launched us without permissions.
  function checkAllowedNoAccess() {
    chrome.extension.isAllowedFileSchemeAccess(pass(function(hasAccess) {
       assertFalse(hasAccess);
    }));
  },

  // Check for no permssions error.
  function captureVisibleNoFile() {
    createWindow([fail_url], kWindowRect, pass(function(winId, tabIds) {
      waitForAllTabs(pass(function() {
        chrome.tabs.getSelected(winId, pass(function(tab) {
          assertEq('complete', tab.status);
          chrome.tabs.captureVisibleTab(winId, fail(
            'Cannot access contents of url "' + fail_url +
            '". Extension manifest must request permission ' +
            'to access this host.'));
        }));
      }));
    }));
  }

]);
