// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ProxySettingsApiTest.ProxyEventsParseError

// TODO(crbug.com/943636): remove old error once
// https://chromium-review.googlesource.com/c/v8/v8/+/1593307 is rolled into
// chromium.

var expected_error_v1 = {
    details: "line: 1: Uncaught SyntaxError: Unexpected token !",
    error: "net::ERR_PAC_SCRIPT_FAILED",
    fatal: false
};

var expected_error_v2 = {
    details: "line: 1: Uncaught SyntaxError: Unexpected token '!'",
    error: "net::ERR_PAC_SCRIPT_FAILED",
    fatal: false
};

function test() {
  // Install error handler and get the test server config.
  chrome.proxy.onProxyError.addListener(function (error) {
    const actualErrorJson = JSON.stringify(error);
    chrome.test.assertTrue(
      actualErrorJson == JSON.stringify(expected_error_v1) ||
      actualErrorJson == JSON.stringify(expected_error_v2),
      actualErrorJson
    );
    chrome.test.notifyPass();
  });

  // Set an invalid PAC script. This should trigger a proxy errors.
  var config = {
    mode: "pac_script",
    pacScript: {
      data: "trash!-FindProxyForURL",
      mandatory: false
    }
  };
  chrome.proxy.settings.set({'value': config}, testDone);
}

function testDone() {
 // Do nothing. The test success/failure is decided in the event handler.
}

test();
