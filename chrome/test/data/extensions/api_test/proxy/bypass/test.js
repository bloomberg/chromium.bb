// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.ProxyBypass

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

chrome.test.runTests([
  function setIndividualProxies() {
    var httpProxy = {
      host: "1.1.1.1"
    };
    var httpProxyExpected = {
      scheme: "http",
      host: "1.1.1.1",
      port: 80
    };

    var rules = {
      proxyForHttp: httpProxy,
      bypassList: ["localhost", "::1", "foo.bar", "<local>"]
    };
    var rulesExpected = {
      proxyForHttp: httpProxyExpected,
      bypassList: ["localhost", "::1", "foo.bar", "<local>"]
    };

    var config = { rules: rules, mode: "fixed_servers" };
    var configExpected = { rules: rulesExpected, mode: "fixed_servers" };

    chrome.experimental.proxy.useCustomProxySettings(config);
    chrome.experimental.proxy.getCurrentProxySettings(
        false,
        expect(configExpected, "invalid proxy settings"));
    chrome.experimental.proxy.getCurrentProxySettings(
        true,
        expect(configExpected, "invalid proxy settings"));
  }
]);
