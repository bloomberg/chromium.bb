// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([
    function testWindowDotPrintCallWorks() {
      chrome.app.window.create('test.html', {}, chrome.test.callbackPass(
          function(appWindow) {
            appWindow.contentWindow.onload = chrome.test.callbackPass(
                function() {
                  appWindow.contentWindow.print();
                });
          }));
    }
  ]);
});
