// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadstop', function(e) {
    chrome.test.sendMessage('guest-loaded');
  });
  webview.setAttribute(
      'src', 'data:text/html,<html><body>tear down test</body></html>');
  document.body.appendChild(webview);
};
