// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var interceptedRequest = false;

chrome.webRequest.onBeforeRequest.addListener(function(details) {
  if (details.url.endsWith('fake_ntp_script.js'))
    interceptedRequest = true;
}, {
  urls: ['<all_urls>'],
});

function getAndResetRequestIntercepted() {
  window.domAutomationController.send(interceptedRequest ? 'true' : 'false');
  interceptedRequest = false;
}

chrome.test.sendMessage('ready');
