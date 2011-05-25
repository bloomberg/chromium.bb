// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ProxySettingsApiTest.ProxyEvents

var captured_events = [];
var expected_events = [
    {
      error: "net::ERR_PROXY_CONNECTION_FAILED",
      details: "",
      fatal: true
    },
    {
      error: "net::ERR_PROXY_CONNECTION_FAILED",
      details: "",
      fatal: true
    },
    {
      error: "net::ERR_PAC_SCRIPT_FAILED",
      details: "line: 1: Uncaught SyntaxError: Unexpected token !",
      fatal: false
    }
  ];

chrome.proxy.onProxyError.addListener(function (error) {
  captured_events.push(error);
  if (captured_events.length < expected_events.length)
    return;
  chrome.test.assertEq(expected_events, captured_events);
  chrome.test.notifyPass();
});

function pacTest(e) {
  var config = {
    mode: "pac_script",
    pacScript: {
      data: "trash!",
      mandatory: false
    }
  };
  chrome.proxy.settings.set({'value': config});
}

var rules = {
  singleProxy: { host: "does.not.exist" }
};

var config = { rules: rules, mode: "fixed_servers" };
chrome.proxy.settings.set({'value': config}, function () {
  var req = new XMLHttpRequest();
  req.open("GET", "http://127.0.0.1/", true);
  req.onload = function () {
    chrome.test.notifyFail("proxy settings should not work");
  }
  req.onerror = pacTest;
  req.send(null);
});
