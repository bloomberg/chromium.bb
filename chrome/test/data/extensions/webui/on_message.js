// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests --gtest_filter=ExtensionWebUITest.OnMessage

chrome.test.listenOnce(chrome.test.onMessage, function(args) {
  if (args.data != 'hi') {
    console.error('Expected "hi", Actual ' + JSON.stringify(args.data));
    chrome.test.sendMessage('false');
  } else {
    chrome.test.sendMessage('true');
  }
});

domAutomationController.send(true);
