// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks the page loaded inside an <adview> has the ability to
// 1) receive "message" events from the application, and 2) use
// "window.postMessage" to post back a message to the application.

function runTests(guestURL) {
  chrome.test.runTests([
    function test() {
      var adview = document.getElementsByTagName("adview")[0];

      adview.addEventListener("loadcommit", function() {
        adview.contentWindow.postMessage({
          message: "onloadcommit",
          data: "data"
        }, "*");
      });

      window.addEventListener("message", function(event) {
        if (event.data.message == "onloadcommit-ack") {
          console.log("onloadcommit-ack message received.");
          chrome.test.succeed();
        }
      });

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
