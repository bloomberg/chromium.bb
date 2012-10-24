// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var url = 'http://localhost:' + config.testServer.port +
      '/files/extensions/platform_apps/web_view_isolation/cookie.html';
  var url2 = 'http://localhost:' + config.testServer.port +
      '/files/extensions/platform_apps/web_view_isolation/cookie2.html';
  var node = document.getElementById('web_view_container');
  node.innerHTML = "<object id='webview' src=" + url +
      " type='application/browser-plugin' width=500 height=550></object>" +
      "<object id='webview2' src=" + url2 +
      " type='application/browser-plugin' width=500 height=550></object>";
  chrome.test.sendMessage('Launched');
});
