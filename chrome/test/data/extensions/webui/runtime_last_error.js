// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests --gtest_filter=ExtensionWebUITest.RuntimeLastError

var success = true;

if (!chrome.test.checkDeepEq({}, chrome.runtime)) {
  console.error('Expected {}, Actual ' + JSON.stringify(chrome.runtime));
  success = false;
}

chrome.test.sendMessage('ping', function(reply) {
  var expected = {
    'lastError': {
      'message': 'unknown host'
    }
  };
  if (!chrome.test.checkDeepEq(expected, chrome.runtime)) {
    console.error('Expected ' + JSON.stringify(expected) + ', ' +
                  'Actual ' + JSON.stringify(chrome.runtime));
    success = false;
  }
  chrome.test.sendMessage(success ? 'true' : 'false');
});

domAutomationController.send(true);
