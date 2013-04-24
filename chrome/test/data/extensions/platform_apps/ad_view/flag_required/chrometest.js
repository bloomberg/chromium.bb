// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks an <adview> element has no behavior when the "kEnableAdview"
// flag is missing.

function runTests() {
  chrome.test.runTests([
    function test() {
      var assertInactive = function(adview) {
        chrome.test.assertEq(undefined, adview['src']);
        chrome.test.assertEq(undefined, adview['ad-network']);
        chrome.test.assertEq(undefined, adview['name']);

        chrome.test.assertEq(false, adview.hasAttribute('src'));
        // 'ad-network' is initially set in index.html.
        chrome.test.assertEq(true, adview.hasAttribute('ad-network'));
        chrome.test.assertEq(false, adview.hasAttribute('name'));

        console.log("Properties and attributes are inactive.");
      };

      var adview = document.getElementsByTagName("adview")[0];
      adview.addEventListener("loadcommit", function() {
        chrome.test.fail('Ad network should not be loaded if adview command ' +
          'line flag is not set.');
      })

      adview.addEventListener("loadabort", function() {
        chrome.test.fail('Ad network should not be loaded if adview command ' +
          'line flag is not set.');
      })

      // Checks that static "ad-network" attribute value has no effect.
      assertInactive(adview);

      // Checks that setting the "ad-network" attribute value has no effect.
      adview.setAttribute('ad-network', 'test');
      // Timeout is necessary to give the mutation observers a chance to fire.
      // http://lists.w3.org/Archives/Public/public-webapps/2011JulSep/1622.html
      setTimeout(function() {
        assertInactive(adview);
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
