// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function (launchData) {
  // Complete correctness of launchData is tested in another test.
  chrome.test.assertTrue(typeof launchData !== 'undefined');

  chrome.app.window.create(
    "main.html",
    {},
    function(win) {
      win.contentWindow.onload = function() {
        // Redirect the embedded webview to the same URL we've been launched
        // with. This should not create an endless loop of redirecting on
        // ourselves with multiplying windows.
        var webview = this.document.getElementById('wv');
        webview.src = launchData.url;

        webview.addEventListener("loadstop", function() {
          // Give webview plenty of time to navigate to make sure that doesn't
          // relaunch the handler.
          setTimeout(function() {
            chrome.test.sendMessage("Handler launched");
          }, 500);
        });
      }
    }.bind(this)
  );
});
