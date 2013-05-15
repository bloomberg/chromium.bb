// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var startTest = function() {
  chrome.test.sendMessage('connected');
};

chrome.test.getConfig(function(config) {
  var guestURL = 'http://localhost:' + config.testServer.port +
      '/files/extensions/platform_apps/web_view/popup_positioning/guest.html';
  var webview = document.createElement('webview');
  var loaded = false;
  webview.addEventListener('loadstop', function(e) {
    window.console.log('webview.loadstop');
    if (!loaded) {
      loaded = true;
      startTest();
    }
  });
  webview.style.width = '200px';
  webview.style.height = '200px';
  webview.setAttribute('src', guestURL);
  document.querySelector('#webview-tag-container').appendChild(webview);
});
