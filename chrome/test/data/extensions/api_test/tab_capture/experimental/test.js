// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabCapture = chrome.tabCapture;

chrome.test.runTests([
  function captureTabAndVerifyStateTransitions() {
    // Tab capture events in the order they happen.
    var tabCaptureEvents = [];

    var tabMediaRequestCallback = function(stream) {
      chrome.test.assertTrue(stream != null);
      stream.stop();
    };

    var tabCaptureListener = function(info) {
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

    tabCapture.capture({audio: true, video: true}, tabMediaRequestCallback);
  },

  function getCapturedTabs() {
    var activeStream = null;

    var capturedTabsAfterClose = function(infos) {
      chrome.test.assertEq(1, infos.length);
      chrome.test.assertEq('stopped', infos[0].status);
      chrome.test.succeed();
    }

    var capturedTabsAfterOpen = function(infos) {
      chrome.test.assertEq(1, infos.length);
      chrome.test.assertEq('active', infos[0].status);
      activeStream.stop();
      tabCapture.getCapturedTabs(capturedTabsAfterClose);
    }

    var tabMediaRequestCallback = function(stream) {
      chrome.test.assertTrue(stream != null);
      activeStream = stream;
      tabCapture.getCapturedTabs(capturedTabsAfterOpen);
    };

    tabCapture.capture({audio: true, video: true}, tabMediaRequestCallback);
  },

  function captureSameTab() {
    var stream1 = null;

    var tabMediaRequestCallback2 = function(stream) {
      chrome.test.assertLastError(
          'Cannot capture a tab with an active stream.');
      chrome.test.assertTrue(stream == null);
      stream1.stop();
      chrome.test.succeed();
    };

    var tabMediaRequestCallback = function(stream) {
      chrome.test.assertTrue(stream != null);
      stream1 = stream;
      tabCapture.capture({audio: true, video: true}, tabMediaRequestCallback2);
    };

    tabCapture.capture({audio: true, video: true}, tabMediaRequestCallback);
  },

  function supportsMediaConstraints() {
    var tabMediaRequestCallback = function(stream) {
      chrome.test.assertTrue(stream != null);
      stream.stop();
      chrome.test.succeed();
    };

    tabCapture.capture({video: true, audio: true,
                        videoConstraints: {
                            mandatory: {
                              maxWidth: 1000,
                              minWidth: 300
                            }
                        },
                        audioConstraints: {
                            mandatory: {
                              minFrameRate: 60
                            }
                        }}, tabMediaRequestCallback);
  },

  function onlyVideo() {
    var tabMediaRequestCallback = function(stream) {
      chrome.test.assertTrue(stream != null);
      stream.stop();
      chrome.test.succeed();
    };

    tabCapture.capture({video: true}, tabMediaRequestCallback);
  },

  function onlyAudio() {
    var tabMediaRequestCallback = function(stream) {
      chrome.test.assertTrue(stream != null);
      stream.stop();
      chrome.test.succeed();
    };

    tabCapture.capture({audio: true}, tabMediaRequestCallback);
  },

  function noAudioOrVideoRequested() {
    var tabMediaRequestCallback = function(stream) {
      chrome.test.assertTrue(stream == null);
      chrome.test.succeed();
    };

    // If not specified, video is not requested.
    tabCapture.capture({audio: false}, tabMediaRequestCallback);
  }

]);
