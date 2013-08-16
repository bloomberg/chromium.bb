// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.loadGuest = function(port) {
  window.console.log('embedder.loadGuest: ' + port);
  var webview = document.createElement('webview');

  // This page is not loaded, we just need a https URL.
  var guestSrcHTTPS = 'https://localhost:' + port +
      '/files/extensions/platform_apps/web_view/' +
      'interstitial_teardown/https_page.html';
  window.console.log('guestSrcHTTPS: ' + guestSrcHTTPS);
  webview.setAttribute('src', guestSrcHTTPS);

  document.body.appendChild(webview);
  chrome.test.sendMessage('GuestAddedToDom');
};

window.onload = function() {
  chrome.test.sendMessage('EmbedderLoaded');
};
