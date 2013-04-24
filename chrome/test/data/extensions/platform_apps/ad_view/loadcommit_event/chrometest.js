// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks the "loadcommit" event is called when the page inside an
// <adview> is loaded.

function runTests(guestURL) {
  chrome.test.runTests([
    function test() {
      var adview = document.getElementsByTagName("adview")[0];

      adview.addEventListener("loadcommit", function(event) {
        var url = event.url;
        var isTopLevel = event.isTopLevel;
        chrome.test.assertEq(guestURL, url);
        chrome.test.assertEq(true, isTopLevel);
        console.log("loadcommit event called: url=" + url);
        chrome.test.succeed();
      })

      adview.setAttribute("src", guestURL);
    }
  ]);
}

window.onload = function() {
  chrome.test.getConfig(function(config) {
    var guestURL = 'http://localhost:' + config.testServer.port +
        '/files/extensions/platform_apps/ad_view/ad_network_site/testsdk.html';
    runTests(guestURL);
  });
}
