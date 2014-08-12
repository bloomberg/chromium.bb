// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;

var WEBVIEW_SRC = "data:text/html,<body>One</body>";

chrome.test.runTests([
  // Tests that embedding <webview> inside a background page loads.
  function inDOM() {
    var webview = document.querySelector('webview');
    webview.addEventListener('contentload', pass());
    webview.setAttribute('src', WEBVIEW_SRC);
  },

  // Tests that creating and attaching a WebView element inside a background
  // page loads.
  function newWebView() {
    var webview = new WebView();
    webview.addEventListener('contentload', pass());
    webview.src = WEBVIEW_SRC;
    document.body.appendChild(webview);
  }
]);
