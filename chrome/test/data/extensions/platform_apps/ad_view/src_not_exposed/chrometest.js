// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks 1) an <adview> works end-to-end (i.e. page is loaded) when
// using a whitelisted ad-network, and 2) the "src" attribute is never exposed
// to the application.

function runTests() {
  chrome.test.runTests([
    function test() {
      var adview = document.getElementsByTagName("adview")[0];
      adview.addEventListener("loadcommit", function(e){
        chrome.test.assertEq(false, adview.hasAttribute("src"));
        chrome.test.assertEq(null, adview.getAttribute("src"));
        chrome.test.assertEq(undefined, adview['src']);
        chrome.test.assertEq(undefined, adview.src);
        console.log("Properties and attributes are inactive.");
        chrome.test.succeed();
      });
      adview.setAttribute('ad-network', 'admob');
    }
  ]);
}

window.onload = function() {
  chrome.test.getConfig(function(config) {
    runTests();
  });
}
