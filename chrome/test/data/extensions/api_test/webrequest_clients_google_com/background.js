// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.webRequestCount = 0;

chrome.webRequest.onBeforeRequest.addListener(function(details) {
  ++window.webRequestCount;
}, {urls:['https://clients1.google.com/']});

chrome.test.sendMessage('ready');
