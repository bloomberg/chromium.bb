// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testTabs = [
    { url: 'a.html', width: 200, height: 200 },
    { url: 'b.html', width: 200, height: 200 }
];

var firstTabId;
var secondTabId;
var firstTabDataURL;
var secondTabDataURL;

var DATA_URL_PREFIX = 'data:image/png;base64,';

chrome.test.runTests([
  function createFirstTab() {
    chrome.test.listenOnce(
        chrome.experimental.offscreenTabs.onUpdated,
        function(tabId, changeInfo, tab) {
      assertSimilarTabs(testTabs[0], tab);
      firstTabId = tab.id;
    });

    chrome.experimental.offscreenTabs.create(
        testTabs[0], pass(function(tab) {
      assertSimilarTabs(testTabs[0], tab);
    }));
  },

  function createSecondTab() {
    chrome.test.listenOnce(
        chrome.experimental.offscreenTabs.onUpdated,
        function(tabId, changeInfo, tab) {
      assertSimilarTabs(testTabs[1], tab);
      secondTabId = tab.id;
    });

    chrome.experimental.offscreenTabs.create(
        testTabs[1], pass(function(tab) {
      assertSimilarTabs(testTabs[1], tab);
    }));
  },

  // Capture an image of the first tab and verify that the data URL looks
  // reasonably correct.
  function captureFirstTab() {
    chrome.experimental.offscreenTabs.toDataUrl(
        firstTabId, {format: 'png'}, pass(function(dataURL) {
      assertEq('string', typeof(dataURL));
      assertEq(DATA_URL_PREFIX, dataURL.substring(0, DATA_URL_PREFIX.length));
      firstTabDataURL = dataURL;
    }));
  },

  // Capture an image of the second tab and verify that the data URL looks
  // reasonably correct.
  function captureSecondTab() {
    chrome.experimental.offscreenTabs.toDataUrl(
        secondTabId, {format: 'png'}, pass(function(dataURL) {
      assertEq('string', typeof(dataURL));
      assertEq(DATA_URL_PREFIX, dataURL.substring(0, DATA_URL_PREFIX.length));
      secondTabDataURL = dataURL;
    }));
  }

  /*
  This test does not work on Mac because the API always returns black images.

  // Make sure the data URLs for the two tabs are different, since pages are
  // different (though the height and widths are equal).
  function compareCaptures() {
    assertFalse(firstTabDataURL == secondTabDataURL);
    chrome.test.succeed();
  }
  */
]);
