// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testSize() {
  var webview = document.querySelector('webview');
  // Since we can't currently inspect the page loaded inside the <webview>,
  // the only way we can check that the shim is working is by changing the
  // size and seeing if the shim updates the size of the DOM.
  chrome.test.assertEq(300, webview.offsetWidth);
  chrome.test.assertEq(200, webview.offsetHeight);

  webview.style.width = '310px';
  webview.style.height = '210px';

  chrome.test.assertEq(310, webview.offsetWidth);
  chrome.test.assertEq(210, webview.offsetHeight);

  webview.style.width = '320px';
  webview.style.height = '220px';

  chrome.test.assertEq(320, webview.offsetWidth);
  chrome.test.assertEq(220, webview.offsetHeight);

  var dynamicWebViewTag = document.createElement('webview');
  dynamicWebViewTag.setAttribute('src', 'data:text/html,dynamic browser');
  dynamicWebViewTag.style.width = '330px';
  dynamicWebViewTag.style.height = '230px';
  document.body.appendChild(dynamicWebViewTag);

  // Timeout is necessary to give the mutation observers a chance to fire.
  setTimeout(function() {
    chrome.test.assertEq(330, dynamicWebViewTag.offsetWidth);
    chrome.test.assertEq(230, dynamicWebViewTag.offsetHeight);

    chrome.test.succeed();
  }, 0);
}

onload = function() {
  chrome.test.runTests([testSize]);
};
