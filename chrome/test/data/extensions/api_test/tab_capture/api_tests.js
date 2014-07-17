// Copyright 2014 The Chromium Authors. All rights reserved.
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
    chrome.tabs.create({active:true}, function(secondTab) {
      // chrome.tabCapture.capture() will only capture the active tab.
      chrome.test.assertTrue(secondTab.active);

      function checkInfoForSecondTabHasStatus(infos, status) {
        for (var i = 0; i < infos.length; ++i) {
          if (infos[i].tabId == secondTab) {
            chrome.test.assertNe(null, status);
            chrome.test.assertEq(status, infos[i].status);
            chrome.test.assertEq(false, infos[i].fullscreen);
            return;
          }
        }
      }

      // Step 4: After the second tab is closed, check that getCapturedTabs()
      // returns no info at all about the second tab.  http://crbug.com/338445
      chrome.tabs.onRemoved.addListener(function() {
        tabCapture.getCapturedTabs(function checkNoInfos(infos) {
          checkInfoForSecondTabHasStatus(infos, null);
          chrome.test.succeed();
        });
      });

      var activeStream = null;

      // Step 3: After the stream is stopped, check that getCapturedTabs()
      // returns 'stopped' capturing status for the second tab.
      var capturedTabsAfterStopCapture = function(infos) {
        checkInfoForSecondTabHasStatus(infos, 'stopped');
        chrome.tabs.remove(secondTab.id);
      };

      // Step 2: After the stream is started, check that getCapturedTabs()
      // returns 'active' capturing status for the second tab.
      var capturedTabsAfterStartCapture = function(infos) {
        checkInfoForSecondTabHasStatus(infos, 'active');
        activeStream.stop();
        tabCapture.getCapturedTabs(capturedTabsAfterStopCapture);
      };

      // Step 1: Start capturing the second tab (the currently active tab).
      tabCapture.capture({audio: true, video: true}, function(stream) {
        chrome.test.assertTrue(!!stream);
        activeStream = stream;
        tabCapture.getCapturedTabs(capturedTabsAfterStartCapture);
      });
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

  function onlyVideo() {
    tabCapture.capture({video: true}, function(stream) {
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
  }

]);
