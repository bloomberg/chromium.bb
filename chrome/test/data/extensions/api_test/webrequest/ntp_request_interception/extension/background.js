// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var urlToIntercept;
var interceptedRequest = false;

chrome.webRequest.onBeforeRequest.addListener(function(details) {
  if (urlToIntercept && details.url === urlToIntercept)
    interceptedRequest = true;
}, {
  urls: ['<all_urls>'],
});

function getAndResetRequestIntercepted() {
  window.domAutomationController.send(interceptedRequest ? 'true' : 'false');
  interceptedRequest = false;
}

chrome.test.sendMessage('ready', function(url) {
  urlToIntercept = url;
});
