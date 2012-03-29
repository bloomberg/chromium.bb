// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks that setting allow_js_access to false is effective:
// - A background page is opened via window.open (which is verified by the
//   AppBackgroundPageApiTest.NoJsBackgroundPage code).
// - The return value of the window.open call is null (since the background
//   page is not scriptable)

var pagePrefix =
    'http://a.com:PORT/files/extensions/api_test/app_background_page/no_js';

// Dispatch "tunneled" functions from the live web pages to this testing page.
chrome.extension.onRequest.addListener(function(request) {
  window[request.name](request.args);
});

// At no point should a window be created that contains the background page
// (bg.html).
chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  if (tab.url.match("bg\.html$")) {
    chrome.test.notifyFail("popup opened instead of background page");
  }
});

// Start the test by opening the first page in the app. This will try to create
// a background page whose name is "bg", which will succeed, but will not return
// a Window object. However, the background contents should load, which will
// then invoke onBackgroundPageLoaded.
window.onload = function() {
  // We wait for window.onload before getting the test config.  If the
  // config is requested before onload, then sometimes onload has already
  // fired by the time chrome.test.getConfig()'s callback runs.
  chrome.test.getConfig(function(config) {
    var launchUrl =
        pagePrefix.replace(/PORT/, config.testServer.port) + '/launch.html';
    chrome.tabs.create({ 'url': launchUrl });
  });
}

function onBackgroundWindowNotNull() {
  chrome.test.notifyFail('Unexpected non-null window.open result');
}

function onBackgroundPageLoaded() {
  chrome.test.notifyPass();
}
