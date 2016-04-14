// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var guestURL = 'http://localhost:' + config.testServer.port +
      '/extensions/platform_apps/web_view/text_input_state/guest.html';
  var webview = document.createElement('webview');
  document.querySelector('#webview-container').appendChild(webview);
  webview.onloadstop = function() {
    chrome.test.sendMessage('connected');
  };
  webview.addEventListener('consolemessage', function(e) {
    chrome.test.sendMessage('GUEST-FOCUSED');
  });
  webview.src = guestURL;
});

window.addEventListener('load', function() {
  document.querySelector('#first').addEventListener('focus', function() {
    chrome.test.sendMessage('EMBEDDER-FOCUSED-1');
  });

  document.querySelector('#second').addEventListener('focus', function() {
    chrome.test.sendMessage('EMBEDDER-FOCUSED-2');
  });
});

function detachWebView() {
  var webview = document.querySelector('webview');
  if (webview) {
    webview.parentNode.removeChild(webview);
    chrome.test.sendMessage('detached');
  } else {
    chrome.test.sendMessage('failed-to-detach');
  }
}

