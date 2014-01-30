// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks that <adview> attributes are also exposed as properties
// (with the same name and value).

function runTests() {
  chrome.test.runTests([
    function test() {
      var adview = document.getElementsByTagName('adview')[0];
      var adnetwork = adview.getAttribute('ad-network');

      // Timeout is necessary to give the mutation observers a chance to fire.
      // http://lists.w3.org/Archives/Public/public-webapps/2011JulSep/1622.html
      setTimeout(function() {
        chrome.test.assertEq(adnetwork, adview.getAttribute('ad-network'));
        chrome.test.assertEq(adnetwork, adview['ad-network']);

        console.log("Properties verified.");
        chrome.test.succeed();
      }, 0);
    }
  ]);
}

window.onload = function() {
  chrome.test.getConfig(function(config) {
    runTests();
  });
}
