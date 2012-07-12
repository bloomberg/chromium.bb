// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var url = 'http://localhost:' + config.testServer.port +
      '/files/extensions/platform_apps/browser_tag_isolation/cookie.html';
  var node = document.getElementById('browser_container');
  node.innerHTML = "<object id='browser' src=" + url +
      " type='application/browser-plugin' width=500 height=550></object>";
  chrome.test.sendMessage('Launched');
});
