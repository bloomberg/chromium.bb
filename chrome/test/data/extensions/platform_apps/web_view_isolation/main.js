// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var url = 'http://localhost:' + config.testServer.port +
      '/files/extensions/platform_apps/web_view_isolation/cookie.html';
  var url2 = 'http://localhost:' + config.testServer.port +
      '/files/extensions/platform_apps/web_view_isolation/cookie2.html';
  var url3 = 'http://localhost:' + config.testServer.port +
      '/files/extensions/platform_apps/web_view_isolation/storage1.html';
  var url4 = 'http://localhost:' + config.testServer.port +
      '/files/extensions/platform_apps/web_view_isolation/storage2.html';
  var url5 = 'http://localhost:' + config.testServer.port +
      '/files/extensions/platform_apps/web_view_isolation/storage1.html#p1';
  var url6 = 'http://localhost:' + config.testServer.port +
      '/files/extensions/platform_apps/web_view_isolation/storage1.html#p2';
  var url7 = 'http://localhost:' + config.testServer.port +
      '/files/extensions/platform_apps/web_view_isolation/storage1.html#p3';
  var node = document.getElementById('web_view_container');
  node.innerHTML =
      "<object id='webview' src=" + url +
      " type='application/browser-plugin' width=500 height=550></object>" +
      "<object id='webview2' src=" + url2 +
      " type='application/browser-plugin' width=500 height=550></object>" +
      "<object id='webview3' partition='partition1' src=" + url3 +
      " type='application/browser-plugin' width=500 height=550></object>" +
      "<object id='webview4' partition='partition1' src=" + url4 +
      " type='application/browser-plugin' width=500 height=550></object>" +
      "<object id='webview5' partition='persist:1' src=" + url5 +
      " type='application/browser-plugin' width=500 height=550></object>" +
      "<object id='webview6' partition='persist:1' src=" + url6 +
      " type='application/browser-plugin' width=500 height=550></object>" +
      "<object id='webview7' partition='persist:2' src=" + url7 +
      " type='application/browser-plugin' width=500 height=550></object>";
  chrome.test.sendMessage('Launched');
});
