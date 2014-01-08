// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaStream = null;
var events = [];

chrome.tabCapture.onStatusChanged.addListener(function(info) {
  if (info.status == 'active') {
    events.push(info.fullscreen);
    if (events.length == 3) {
      chrome.test.assertFalse(events[0]);
      chrome.test.assertTrue(events[1]);
      chrome.test.assertFalse(events[2]);
      mediaStream.stop();
      chrome.test.succeed();
    }
  }
});

chrome.tabCapture.capture({audio: true, video: true}, function(stream) {
  chrome.test.assertTrue(!!stream);
  mediaStream = stream;

  chrome.test.notifyPass();
  chrome.test.sendMessage('ready1', function() {
    chrome.test.sendMessage('ready2', function() {});
  });
});
