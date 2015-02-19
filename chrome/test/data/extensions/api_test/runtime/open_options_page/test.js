// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// browser_tests --gtest_filter=ExtensionApiTest.OpenOptionsPage

var assertEq = chrome.test.assertEq;

function test() {
  chrome.test.listenOnce(chrome.runtime.onMessage, function(m, sender) {
    assertEq('success', m);
    assertEq(chrome.runtime.id, sender.id);
    assertEq(chrome.runtime.getURL('options.html'), sender.url);
  });
  chrome.runtime.openOptionsPage();
}

chrome.test.runTests([test]);
