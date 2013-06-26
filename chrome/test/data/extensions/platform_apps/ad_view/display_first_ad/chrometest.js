// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks the page running inside an <adview> has the ability to load
// and display an image inside an <iframe>.

function runTests(guestURL) {
  chrome.test.runTests([
    function test() {
      var adview = document.getElementsByTagName("adview")[0];

      adview.addEventListener("loadcommit", function() {
        adview.contentWindow.postMessage({
          message: "display-first-ad",
          publisherData: "data"
        }, "*");
      })

      window.addEventListener("message", function(event) {
        if (event.data.message == "ad-displayed") {
          console.log("ad-displayed: " + event.data.data.adSize.height);
          chrome.test.succeed();
        }
      })

      adview.setAttribute("src", guestURL);
    }
  ]);
}

window.onload = function() {
  chrome.test.getConfig(function(config) {
    var guestURL = 'http://localhost:' + config.testServer.port +
        '/extensions/platform_apps/ad_view/ad_network_site/testsdk.html';
    runTests(guestURL);
  });
}
