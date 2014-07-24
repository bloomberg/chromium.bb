// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests --gtest_filter=ExtensionWebUITest.SendMessage

chrome.test.sendMessage('ping', function(reply) {
  if (reply != 'pong') {
    console.error('Expected "pong", Actual ' + JSON.stringify(reply));
    chrome.test.sendMessage('false');
  } else {
    chrome.test.sendMessage('true');
  }
});

domAutomationController.send(true);
