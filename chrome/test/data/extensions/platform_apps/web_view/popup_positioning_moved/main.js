// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var startTest = function() {
  chrome.test.sendMessage('connected');
};

chrome.test.getConfig(function(config) {
  var guestURL = 'http://localhost:' + config.testServer.port +
      '/files/extensions/platform_apps/web_view/popup_positioning_moved' +
      '/guest.html';
  var webview = document.createElement('webview');
  var loaded = false;


  var onWebViewLoadStop = function(e) {
    window.console.log('webview.loadstop');
    if (loaded) {
      return;
    }
    loaded = true;
    // We move the <webview> in a way, this would trigger
    // BrowserPlugin::updateGeometry but no UpdateRects.
    webview.style.paddingLeft = '20px';
    startTest();
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);

  webview.style.width = '200px';
  webview.style.height = '200px';
  webview.setAttribute('src', guestURL);
  document.querySelector('#webview-tag-container').appendChild(webview);
});
