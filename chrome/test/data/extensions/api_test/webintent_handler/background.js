// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
This extension is a platform app that provides a Web Intent handler; it accepts
incoming requests and invokes chrome.test.succeed() immediately.
*/

function launchedListener(launchData) {
  chrome.test.assertFalse(!launchData, "No launchData");
  chrome.test.assertFalse(!launchData.intent, "No launchData.intent");
  chrome.test.assertEq(launchData.intent.action,
      "http://webintents.org/view");
  chrome.test.assertEq(launchData.intent.type,
      "chrome-extension://fileentry");
  chrome.test.assertFalse(!launchData.intent.data,
      "No launchData.intent.data");

  launchData.intent.data.file(function(file) {
    var reader = new FileReader();
    reader.onloadend = function(e) {
      chrome.test.assertEq("hello, world!", reader.result);
      chrome.test.succeed();
    };
    reader.onerror = function(e) {
      chrome.test.fail("Error reading file contents.");
    };
    reader.readAsText(file);
  });
}

chrome.app.runtime.onLaunched.addListener(launchedListener);
