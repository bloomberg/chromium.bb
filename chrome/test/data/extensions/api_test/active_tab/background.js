// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var theOnlyTestDone = null;

function inject(callback) {
  chrome.tabs.executeScript({ code: "true" }, callback);
}

chrome.browserAction.onClicked.addListener(function() {
  inject(function() {
    chrome.test.assertNoLastError();
    chrome.test.notifyPass();
  });
});

chrome.webNavigation.onCompleted.addListener(function(details) {
  inject(function() {
    chrome.test.assertLastError('Cannot access contents of url "' +
        details.url +
        '". Extension manifest must request permission to access this host.');
    if (details.url.indexOf("final_page") >= 0)
      theOnlyTestDone();
    else
      chrome.test.notifyPass();
  });
});

chrome.test.runTests([
  function theOnlyTest() {
    // This will keep the test alive until the final callback is run.
    theOnlyTestDone = chrome.test.callbackAdded();
  }
]);
