// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks an invalid "ad-network" value (i.e. not whitelisted)
// is ignored.

var adview1 = "adview1";
var adview2 = "adview2";
var adview3 = "adview3";

/**
 * Verify setting invalid ad-network value is ignored in immediate Javascript
 * code (i.e. before document is fully loaded) on a static <adview> element.
 */
function runTest1() {
  var adview = document.getElementById(adview1);

  adview.setAttribute("ad-network", "invalidAdNetwork");

  // Timeout is necessary to give the mutation observers a chance to fire.
  // http://lists.w3.org/Archives/Public/public-webapps/2011JulSep/1622.html
  setTimeout(function() {
    chrome.test.assertEq("", adview.getAttribute("ad-network"));
    console.log("test1 succeeded.");
  }, 0);
}

/**
 * Verify setting invalid ad-network value is ignored at "document load" time.
 */
function runTest2(nextTest) {
  var adview = document.createElement("adview");
  adview.id = adview2;
  document.body.appendChild(adview);

  adview.setAttribute("ad-network", "invalidAdNetwork2");

  // Timeout is necessary to give the mutation observers a chance to fire.
  // http://lists.w3.org/Archives/Public/public-webapps/2011JulSep/1622.html
  setTimeout(function() {
    chrome.test.assertEq("", adview.getAttribute("ad-network"));
    console.log("test2 succeeded.");
    nextTest();
  }, 0);
}

/**
 * Verify setting invalid ad-network value is ignored even after document
 * is fully loaded.
 */
function runTest3() {
  // Timeout to ensure the code runs after the "loaded" event.
  setTimeout(function() {
    var adview = document.createElement("adview");
    adview.id = adview3;
    document.body.appendChild(adview);

    adview.setAttribute("ad-network", "invalidAdNetwork3");

    // Timeout is necessary to give the mutation observers a chance to fire.
    // http://lists.w3.org/Archives/Public/public-webapps/2011JulSep/1622.html
    setTimeout(function() {
      chrome.test.assertEq("", adview.getAttribute("ad-network"));
      console.log("test3 succeeded.");
      chrome.test.succeed();
    }, 0);
  }, 0);
}

runTest1();

window.addEventListener('DOMContentLoaded', function() {
  // Pass "runTest3" as "nextTest" to ensure runTest2 fully finishes
  // before test3 is started.
  runTest2(runTest3);
});
