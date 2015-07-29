// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var logForDebugging = true;
function log(message) {
  if (logForDebugging)
    console.log(message);
}

chrome.test.runTests([
  function registerServiceWorker() {
    navigator.serviceWorker.register('/sw.js').then(function(registration) {
      // Registration was successful
      log('ServiceWorker registration successful with scope: ',
          registration.scope);
      chrome.test.succeed();
    }).catch(function(err) {
      // registration failed :(
      var errorMsg = err.name + ': ' + err.message;
      log('ServiceWorker registration failed: ' + errorMsg);
      chrome.test.sendMessage(errorMsg);
      chrome.test.fail();
    });
  }
]);
