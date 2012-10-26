// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabCapture = chrome.tabCapture;

chrome.test.runTests([

  function captureInvalidTab() {
    var tabMediaRequestCallback = function(stream) {
      chrome.test.assertEq(undefined, stream);
      chrome.test.assertLastError('Could not find the specified tab.');
      chrome.test.succeed();
    };

    tabCapture.capture(-1, {audio: true, video: true},
      tabMediaRequestCallback);
  },

  function captureTabAndVerifyStateTransitions() {
    var tabId = -1;
    // Tab capture events in the order they happen.
    var tabCaptureEvents = [];

    var tabMediaRequestCallback = function(stream) {
      chrome.test.assertTrue(stream !== undefined);
      stream.stop();
    };

    var tabCaptureListener = function(info) {
      chrome.test.assertEq(tabId, info.tabId);
      console.log(info.status);
      if (info.status == 'stopped') {
        chrome.test.assertEq('active', tabCaptureEvents.pop());
        chrome.test.assertEq('pending', tabCaptureEvents.pop());
        chrome.test.assertEq('requested', tabCaptureEvents.pop());
        tabCapture.onStatusChanged.removeListener(tabCaptureListener);
        chrome.test.succeed();
        return;
      }
      tabCaptureEvents.push(info.status);
    }

    tabCapture.onStatusChanged.addListener(tabCaptureListener);

    var gotTabId = function(tab) {
      tabId = tab[0].id;
      console.log('using tab: ' + tabId);
      tabCapture.capture(tabId, {audio: true, video: true},
        tabMediaRequestCallback);
    };
    chrome.tabs.query({active: true}, gotTabId);
  },

  function getCapturedTabs() {
    var tabId = -1;
    var activeStream = null;

    var capturedTabsAfterClose = function(infos) {
      chrome.test.assertEq(1, infos.length);
      chrome.test.assertEq(tabId, infos[0].tabId);
      chrome.test.assertEq('stopped', infos[0].status);
      chrome.test.succeed();
    }

    var capturedTabsAfterOpen = function(infos) {
      chrome.test.assertEq(1, infos.length);
      chrome.test.assertEq(tabId, infos[0].tabId);
      chrome.test.assertEq('active', infos[0].status);
      activeStream.stop();
      tabCapture.getCapturedTabs(capturedTabsAfterClose);
    }

    var tabMediaRequestCallback = function(stream) {
      chrome.test.assertTrue(stream !== undefined);
      activeStream = stream;
      tabCapture.getCapturedTabs(capturedTabsAfterOpen);
    };

    var gotTabId = function(tab) {
      tabId = tab[0].id;
      console.log('using tab: ' + tabId);
      tabCapture.capture(tabId, {audio: true, video: true},
        tabMediaRequestCallback);
    };
    chrome.tabs.query({active: true}, gotTabId);
  },

  function captureSameTab() {
    var tabId = -1;
    var stream1 = null;

    var tabMediaRequestCallback2 = function(stream) {
      chrome.test.assertLastError(
          'Cannot capture a tab with an active stream.');
      chrome.test.assertTrue(stream === undefined);
      stream1.stop();
      chrome.test.succeed();
    };

    var tabMediaRequestCallback = function(stream) {
      chrome.test.assertTrue(stream !== undefined);
      stream1 = stream;
      tabCapture.capture(tabId, {audio: true, video: true},
        tabMediaRequestCallback2);
    };

    var gotTabId = function(tab) {
      tabId = tab[0].id;
      console.log('using tab: ' + tabId);
      tabCapture.capture(tabId, {audio: true, video: true},
        tabMediaRequestCallback);
    };
    chrome.tabs.query({active: true}, gotTabId);
  },

]);
