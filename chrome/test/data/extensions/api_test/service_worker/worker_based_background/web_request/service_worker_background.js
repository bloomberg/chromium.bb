// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var baseUrl;

chrome.test.runTests([
  function setup() {
    chrome.test.getConfig(function(config) {
      baseUrl = `http://a.com:${config.testServer.port}/extensions` +
          '/api_test/service_worker/worker_based_background/web_request/';
      chrome.test.succeed();
    });
  },
  // Test the onBeforeRequest listener.
  function testOnBeforeRequest() {
    updateUrl = baseUrl + 'empty.html';
    chrome.webRequest.onBeforeRequest.addListener(
        function localListener(details) {
          chrome.webRequest.onBeforeRequest.removeListener(localListener);
          chrome.test.succeed();
          return {cancel: false};
        }, { urls: ['<all_urls>']});
    // Create the tab.
    chrome.tabs.create({url: updateUrl});
  },
]);
