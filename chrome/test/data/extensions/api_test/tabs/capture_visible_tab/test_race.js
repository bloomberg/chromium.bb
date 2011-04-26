// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// API test for chrome.tabs.captureVisibleTab() that tests that concurrent
// capture requests end up with the expected data (by opening 10 windows with
// alternating black and white contents and asserting that the captured pixels
// are of the expected colors).
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureVisibleTabRace

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

var kWindowRect = {
  'width': 400,
  'height': 400,
};

chrome.test.runTests([
  function captureVisibleTabRace() {
    var windowIdToExpectedColor = {};
    for (var i = 0; i < 10; i++) {
      var colorName;
      var expectedColor;
      if (i % 2) {
        colorName = 'white';
        expectedColor = '255,255,255,255';
      } else {
        colorName = 'black';
        expectedColor = '0,0,0,255';
      }
      var url = chrome.extension.getURL('/common/' + colorName + '.html');
      createWindow(
          [url],
          kWindowRect,
          pass((function(expectedColor, winId, tabIds) {
            windowIdToExpectedColor[winId] = expectedColor;
          }).bind(this, expectedColor)));
    }

    waitForAllTabs(pass(function() {
      for (var windowId in windowIdToExpectedColor) {
        var expectedColor = windowIdToExpectedColor[windowId];
        windowId = parseInt(windowId, 10);
        chrome.tabs.captureVisibleTab(
            windowId,
            pass((function(expectedColor, imgDataUrl) {
              testPixelsAreExpectedColor(
                  imgDataUrl, kWindowRect, expectedColor);
            }).bind(this, expectedColor)));
      }
    }));
 }
]);
