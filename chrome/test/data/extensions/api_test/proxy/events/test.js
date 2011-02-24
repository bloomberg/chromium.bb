// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.ProxyEvents

chrome.experimental.proxy.onProxyError.addListener(function (error) {
  chrome.test.assertTrue(error.fatal);
  chrome.test.assertEq("net::ERR_PROXY_CONNECTION_FAILED", error.error);
  chrome.test.assertEq("", error.details);
  chrome.test.notifyPass();
});

var rules = {
  singleProxy: { host: "does.not.exist" }
};

var config = { rules: rules, mode: "fixed_servers" };
chrome.experimental.proxy.settings.set(
    {'value': config},
    chrome.test.callbackPass());

var req = new XMLHttpRequest();
req.open("GET", "http://127.0.0.1/", true);
req.onload = function () { chrome.test.notifyFail(); }
req.send(null);
