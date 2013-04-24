// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks the "loadabort" event is called when the "src" attribute
// of an <adview> is an invalid URL.

function runTests(guestURL) {
  chrome.test.runTests([
    function test() {
      var adview = document.getElementsByTagName("adview")[0];

      adview.addEventListener("loadabort", function(event) {
        var url = event.url;
        var isTopLevel = event.isTopLevel;
        chrome.test.assertEq(guestURL, url);
        chrome.test.assertEq(true, isTopLevel);
        console.log("loadabort event called: url=" + url);
        chrome.test.succeed();
      })

      adview.setAttribute("src", guestURL);
    }
  ]);
}

window.onload = function() {
  chrome.test.getConfig(function(config) {
    var guestURL = 'http://255.255.255.255/fake_file.html';
    runTests(guestURL);
  });
}
