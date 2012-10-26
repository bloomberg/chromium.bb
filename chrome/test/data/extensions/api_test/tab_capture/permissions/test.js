// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabCapture = chrome.tabCapture;

chrome.test.runTests([

  function cannotDirectlyUseGetUserMedia() {
    var failure = function() {
      chrome.test.succeed();
    };

    var success = function(stream) {
      chrome.test.fail();
    };

    navigator.webkitGetUserMedia({
      video: {
        mandatory: { chromeMediaSource: 'tab', chromeMediaSourceId: '1:2' },
      },
      audio: {
        mandatory: { chromeMediaSource: 'tab', chromeMediaSourceId: '1:2' },
      }
    }, success, failure);
  },

]);
