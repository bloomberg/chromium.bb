// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function (launchData) {
  // Test that the id and items fields in FileEntry can be read.
  chrome.test.runTests([
    function testUrlHandler() {
      chrome.test.assertTrue(typeof launchData != 'undefined', "No launchData");
      chrome.test.assertEq(launchData.id, "my_doc_url",
          "launchData.id incorrect");
      chrome.test.assertTrue(typeof launchData.items == 'undefined',
          "Launched for file_handlers, not url_handlers");
      chrome.test.assertTrue(typeof launchData.url != 'undefined',
          "No url in launchData");
      chrome.test.assertTrue(typeof launchData.referrerUrl != 'undefined',
          "No referrerUrl in launchData");
      chrome.test.assertFalse(
          !launchData.url.match(
               /http:\/\/.*:.*\/extensions\/platform_apps\/url_handlers\/.*/),
          "Wrong launchData.url");
    }
  ]);

  chrome.app.window.create(
    "main.html",
    { },
    function(win) {
      win.contentWindow.onload = function() {
        var link = this.document.querySelector('#link');
        link.href = launchData.url;
        link.innerText = launchData.url;
        chrome.test.sendMessage("Handler launched");
      }
    });
});
