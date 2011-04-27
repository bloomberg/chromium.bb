// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// API test for chrome.tabs.captureVisibleTab() that tests that concurrent
// capture requests end up with the expected data (by opening 8 windows with
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
    // Simulate a callback being added to make sure that the test isn't
    // considered complete until all of the 8 windows are test (this happens
    // in parallel, so the normal nesting of pass() call is not sufficient).
    var callbackCompleted = chrome.test.callbackAdded();

    console.log(new Date() + ' - creating windows');

    var windowsAndColors = [];
    for (var i = 0; i < 8; i++) {
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
          (function(expectedColor, winId, tabIds) {
            console.log(new Date() + ' -   created ' + winId);
            windowsAndColors.push([winId, expectedColor]);
          }).bind(this, expectedColor));
    }

    waitForAllTabs(function() {
      console.log(new Date() + ' - capturing contents');

      var testedWindowCount = 0;
      windowsAndColors.forEach(function(windowIdAndExpectedColor) {
        var windowId = windowIdAndExpectedColor[0];
        var expectedColor = windowIdAndExpectedColor[1];
        chrome.tabs.captureVisibleTab(
            windowId,
            {'format': 'png'},
            function(imgDataUrl) {
              console.log(new Date() + ' -   captured ' + windowId);
              testPixelsAreExpectedColor(
                  imgDataUrl, kWindowRect, expectedColor);
              console.log(new Date() + ' -   tested pixels for ' + windowId);
              testedWindowCount++;
              if (testedWindowCount == windowsAndColors.length) {
                callbackCompleted();
              }
            });
      });
    });
 }
]);
