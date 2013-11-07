// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test makes sure webview properly loads and content script is run.
onload = function() {
  var contentScriptRan = false;
  var webviewLoaded = false;

  var maybePassTest = function() {
    if (contentScriptRan && webviewLoaded) {
      chrome.test.sendMessage('TEST_PASSED');
    }
  };

  var element = document.getElementById('the-bridge-element');
  if (element.innerText == 'Modified') {
    contentScriptRan = true;
    maybePassTest();
  } else {
    // Wait for content script to fire an event.
    element.addEventListener('bridge-event', function(e) {
      contentScriptRan = true;
      maybePassTest();
    });
  }

  var webview = document.createElement('webview');
  webview.addEventListener('loadstop', function(e) {
    webviewLoaded = true;
    maybePassTest();
  });
  webview.setAttribute(
      'src', 'data:text/html,<html><body>tear down test</body></html>');
  document.body.appendChild(webview);
};
