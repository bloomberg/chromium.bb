// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function browserTag() {
      var browserTag = document.querySelector('browser');
      // Since we can't currently inspect the page loaded inside the <browser>,
      // the only way we can check that the shim is working is by changing the
      // size and seeing if the shim updates the size of the DOM.
      chrome.test.assertEq(300, browserTag.offsetWidth);
      chrome.test.assertEq(200, browserTag.offsetHeight);

      browserTag.setAttribute('width', 310);
      browserTag.setAttribute('height', 210);

      // Timeout is necessary to give the mutation observers a chance to fire.
      setTimeout(function() {
        chrome.test.assertEq(310, browserTag.offsetWidth);
        chrome.test.assertEq(210, browserTag.offsetHeight);

        // Should also be able to query/update the dimensions via getterts/
        // setters.
        chrome.test.assertEq(310, browserTag.width);
        chrome.test.assertEq(210, browserTag.height);

        browserTag.width = 320;
        browserTag.height = 220;

        // Setters also end up operating via mutation observers.
        setTimeout(function() {
          chrome.test.assertEq(320, browserTag.offsetWidth);
          chrome.test.assertEq(220, browserTag.offsetHeight);

          chrome.test.succeed();
        }, 0);
      }, 0);
    }
  ]);
};
