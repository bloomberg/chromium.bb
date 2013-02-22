// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabCapture = chrome.tabCapture;

chrome.test.runTests([
  function captureTabAndVerifyStateTransitions() {
    // Tab capture events in the order they happen.
    var tabCaptureEvents = [];

    var tabCaptureListener = function(info) {
      console.log(info.status);
      if (info.status == 'stopped') {
        chrome.test.assertEq('active', tabCaptureEvents.pop());
        chrome.test.assertEq('pending', tabCaptureEvents.pop());
        tabCapture.onStatusChanged.removeListener(tabCaptureListener);
        chrome.test.succeed();
        return;
      }
      tabCaptureEvents.push(info.status);
    };
    tabCapture.onStatusChanged.addListener(tabCaptureListener);

    tabCapture.capture({audio: true, video: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      stream.stop();
    });
  },

  function getCapturedTabs() {
    var activeStream = null;

    var capturedTabsAfterClose = function(infos) {
      chrome.test.assertEq(1, infos.length);
      chrome.test.assertEq('stopped', infos[0].status);
      chrome.test.succeed();
    };

    var capturedTabsAfterOpen = function(infos) {
      chrome.test.assertEq(1, infos.length);
      chrome.test.assertEq('active', infos[0].status);
      activeStream.stop();
      tabCapture.getCapturedTabs(capturedTabsAfterClose);
    };

    tabCapture.capture({audio: true, video: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      activeStream = stream;
      tabCapture.getCapturedTabs(capturedTabsAfterOpen);
    });
  },

  function captureSameTab() {
    var stream1 = null;

    var tabMediaRequestCallback2 = function(stream) {
      chrome.test.assertLastError(
          'Cannot capture a tab with an active stream.');
      chrome.test.assertTrue(!stream);
      stream1.stop();
      chrome.test.succeed();
    };

    tabCapture.capture({audio: true, video: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      stream1 = stream;
      tabCapture.capture({audio: true, video: true}, tabMediaRequestCallback2);
    });
  },

  function supportsMediaConstraints() {
    tabCapture.capture({video: true, audio: true,
                        videoConstraints: {
                            mandatory: {
                              maxWidth: 1000,
                              minWidth: 300
                            }
                        }
                       }, function(stream) {
                          chrome.test.assertTrue(!!stream);
                          stream.stop();
                          chrome.test.succeed();
                       });
  },

  function onlyVideo() {
    tabCapture.capture({video: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      stream.stop();
      chrome.test.succeed();
    });
  },

  function onlyAudio() {
    tabCapture.capture({audio: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      stream.stop();
      chrome.test.succeed();
    });
  },

  function noAudioOrVideoRequested() {
    // If not specified, video is not requested.
    tabCapture.capture({audio: false}, function(stream) {
      chrome.test.assertTrue(!stream);
      chrome.test.succeed();
    });
  },

  function invalidAudioConstraints() {
    var tabCaptureListener = function(info) {
      if (info.status == 'stopped') {
        tabCapture.onStatusChanged.removeListener(tabCaptureListener);

        tabCapture.capture({audio: true, video: true}, function(stream) {
          chrome.test.assertTrue(!!stream);
          chrome.test.succeed();
        });
      }
    };
    tabCapture.onStatusChanged.addListener(tabCaptureListener);

    tabCapture.capture({audio: true,
                        audioConstraints: {
                          'mandatory': {
                            'notValid': '123'
                          }
                        }}, function(stream) {
                          chrome.test.assertTrue(!stream);
                        });
  }

]);
