// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var guestURL = 'http://localhost:' + config.testServer.port +
      '/files/extensions/platform_apps/ad_view/ad_network/testsdk.html';
  var adview = document.getElementsByTagName("adview")[0];

  adview.addEventListener("loadcommit", function() {
    chrome.test.sendMessage('guest-loaded');
  })

  adview.setAttribute("src", guestURL);
});
