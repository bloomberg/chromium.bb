// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks that 1) it is possible change the value of the "ad-network"
// attribute of an <adview> element and 2) changing the value will reset the
// "src" attribute.

function onLoadCommit(adview, guestURL) {
  console.log('onLoadCommit("' + adview.getAttribute('ad-network') + '")');
  // First callback: assert and run another callback
  if (adview.getAttribute('ad-network') == 'test') {
    // Check 'src' and 'ad-network' attributes/properties
    chrome.test.assertEq('test', adview.getAttribute('ad-network'));
    chrome.test.assertEq('test', adview['ad-network']);
    chrome.test.assertEq(guestURL, adview.getAttribute('src'));
    chrome.test.assertEq(guestURL, adview.src);

    adview['ad-network'] = '';
    // Timeout is necessary to give the mutation observers a chance to fire.
    // http://lists.w3.org/Archives/Public/public-webapps/2011JulSep/1622.html
    setTimeout(function() {
      // Check 'src' and 'ad-network' attributes/properties
      chrome.test.assertEq('', adview.getAttribute('ad-network'));
      chrome.test.assertEq('', adview['ad-network']);
      chrome.test.assertEq('', adview.getAttribute('src'));
      chrome.test.assertEq('', adview.src);

      // Assigning a new value to raise a 2nd "loadcommit" event
      adview['ad-network'] = 'test2';
    }, 0);
  }
  // Second callback: assert and finish test
  else if (adview.getAttribute('ad-network') == 'test2') {
    // Check 'src' and 'ad-network' attributes/properties
    chrome.test.assertEq('test2', adview.getAttribute('ad-network'));
    chrome.test.assertEq('test2', adview['ad-network']);
    chrome.test.assertEq('', adview.getAttribute('src'));
    chrome.test.assertEq('', adview['src']);

    console.log('Test finished correctly.');
    chrome.test.succeed();
  }
};

function runTests(guestURL) {
  chrome.test.runTests([
    function test() {
      var adview = document.getElementsByTagName('adview')[0];

      adview.addEventListener('loadcommit', function(event) {
        onLoadCommit(adview, guestURL);
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
