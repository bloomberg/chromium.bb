// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) { window.console.log(msg); };

// window.* exported functions begin.
window.setTransform = function(tr) {
  LOG('Setting transform to "' + tr + '".');
  var webview = document.body.querySelector('webview');
  webview.style.transform = (tr !== 'NONE') ? tr : '';
  webview.onloadstop = function() {
    LOG('Transform "' + tr + '" is set.');
    chrome.test.sendMessage('TRANSFORM_SET');
  };
  webview.reload();
};
// window.* exported functions end.

function setUpTest(messageCallback) {
  var guestUrl = 'data:text/html,<html><body>guest</body></html>';
  var webview = document.createElement('webview');
  var onLoadStop = function(e) {
    LOG('webview has loaded.');
    messageCallback(webview);
  };
  webview.addEventListener('loadstop', onLoadStop);

  webview.setAttribute('src', guestUrl);
  document.body.appendChild(webview);
}

onload = function() {
  chrome.test.getConfig(function(config) {
    setUpTest(function(webview) {
      chrome.test.sendMessage('Launched');
    });
  });
};
