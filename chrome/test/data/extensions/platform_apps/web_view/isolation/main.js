// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.sendMessage('Launched');

chrome.test.getConfig(function(config) {
  var url = 'http://localhost:' + config.testServer.port +
      '/extensions/platform_apps/web_view/isolation/cookie.html';
  var url2 = 'http://localhost:' + config.testServer.port +
      '/extensions/platform_apps/web_view/isolation/cookie2.html';
  var url3 = 'http://localhost:' + config.testServer.port +
      '/extensions/platform_apps/web_view/isolation/storage1.html';
  var url4 = 'http://localhost:' + config.testServer.port +
      '/extensions/platform_apps/web_view/isolation/storage2.html';
  var url5 = 'http://localhost:' + config.testServer.port +
      '/extensions/platform_apps/web_view/isolation/storage1.html#p1';
  var url6 = 'http://localhost:' + config.testServer.port +
      '/extensions/platform_apps/web_view/isolation/storage1.html#p2';
  var url7 = 'http://localhost:' + config.testServer.port +
      '/extensions/platform_apps/web_view/isolation/storage1.html#p3';
  var node = document.getElementById('web_view_container');
  node.innerHTML =
      "<webview id='webview' src='" + url + "'></webview>" +
      "<webview id='webview2' src='" + url2 + "'></webview>" +
      "<webview id='webview3' partition='partition1' src='" + url3 +
      "'></webview>" +
      "<webview id='webview4' partition='partition1' src='" + url4 +
      "'></webview>" +
      "<webview id='webview5' partition='persist:1' src='" + url5 +
      "'></webview>" +
      "<webview id='webview6' partition='persist:1' src='" + url6 +
      "'></webview>" +
      "<webview id='webview7' partition='persist:2' src='" + url7 +
      "'></webview>";
});
