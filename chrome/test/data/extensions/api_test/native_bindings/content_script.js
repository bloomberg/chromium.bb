// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener((port) => {
  port.onMessage.addListener((message) => {
    chrome.test.assertEq('background page', message);
    port.postMessage('content script');
  });
});

chrome.runtime.sendMessage('startFlow', function(response) {
  chrome.test.assertEq('started', response);
});
