// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks the "src" attribute is ignored when the
// "kEnableAdviewSrcAttribute" is missing.

function runTests(guestURL) {
  chrome.test.runTests([
    function test() {
      var adview = document.getElementsByTagName("adview")[0];

      adview.setAttribute("src", guestURL);

      // Timeout is necessary to give the mutation observers a chance to fire.
      // http://lists.w3.org/Archives/Public/public-webapps/2011JulSep/1622.html
      setTimeout(function() {
        chrome.test.assertEq("", adview.getAttribute("src"));
        chrome.test.assertEq(undefined, adview['src']);
        chrome.test.assertEq(undefined, adview.src);
        console.log("Properties and attributes are inactive.");
        chrome.test.succeed();
      }, 0);
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
