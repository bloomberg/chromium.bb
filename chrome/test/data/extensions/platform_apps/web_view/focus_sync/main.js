// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function $(id) {
  return document.getElementById(id);
}

window.reloadWebview = function() {
    $('webview').reload();
    $('webview').focus();
};

window.addEventListener('DOMContentLoaded', function() {
  var webview = document.querySelector('#webview');
  webview.addEventListener('loadstop', function() {
      chrome.test.sendMessage('WebViewTest.LAUNCHED');
  });
  webview.focus();
  webview.src = 'focus_test.html';
});
