// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var webview = null;
var loadcount = 0;

var startTest = function() {
  window.addEventListener('message', receiveMessage, false);
  chrome.test.sendMessage('guest-loaded');
  webview = document.getElementById('webview');
  webview.addEventListener('loadstop', onWebviewLoaded);
};

var onWebviewLoaded = function(event) {
  loadcount++;
  webview.contentWindow.postMessage('msg', '*');
}

var receiveMessage = function(event) {
  if (loadcount == 1) {
    var webviewContainer = document.querySelector('#webview-tag-container')
    webviewContainer.removeChild(webview);
    webviewContainer.appendChild(webview);
  } else if (loadcount == 2) {
    chrome.test.succeed();
  } else {
    chrome.test.fail();
  }
}

chrome.test.getConfig(function(config) {
  var guestURL = 'http://localhost:' + config.testServer.port +
      '/extensions/platform_apps/web_view/addremove/guest.html';
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview id=\'webview\' style="width: 400px; height: 400px; ' +
      'margin: 0; padding: 0;"' +
      ' src="' + guestURL + '"' +
      '></webview>';
  startTest();
});
