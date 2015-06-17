// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(xunjieli): When URLSearchParams is stable and implemented, switch this
// (and a lot of other test code) to it. https://crbug.com/303152
var url = decodeURIComponent(/url=([^&]*)/.exec(location.search)[1]);
var filter = {urls: ['http://www.example.com/*'], types: ['xmlhttprequest']};
var numSuccessfulRequests = 0;
var numThrottledRequests = 0;

chrome.webRequest.onCompleted.addListener(function(details) {
  numSuccessfulRequests++;
  chrome.test.assertEq(0, numThrottledRequests);
  chrome.test.assertEq(503, details.statusCode);
  chrome.runtime.sendMessage({type: 'xhr', method: 'GET', url: url});
}, filter);

chrome.webRequest.onErrorOccurred.addListener(function(details) {
  numThrottledRequests++;
  // Should not throttle the first request.
  chrome.test.assertTrue(numSuccessfulRequests > 1);
  chrome.test.assertEq('net::ERR_TEMPORARILY_THROTTLED', details.error);
  // Send 10 requests in a row so that backoff time is large enough to avoid
  // flakiness on slow bots. See: crbug.com/501362.
  if (numThrottledRequests < 10) {
    chrome.runtime.sendMessage({type: 'xhr', method: 'GET', url: url});
  } else {
    chrome.test.notifyPass();
  }
}, filter);

chrome.runtime.sendMessage({type: 'xhr', method: 'GET', url: url});
