// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.ProxyManualSingle

chrome.test.runTests([
  function setSingleProxy() {
    var oneProxy = {
      scheme: "http",
      host: "127.0.0.1",
      port: 100
    };

    // Single proxy should override HTTP proxy.
    var httpProxy = {
      host: "8.8.8.8"
    };

    // Single proxy should not override SOCKS proxy.
    var socksProxy = {
      host: "9.9.9.9"
    };

    var rules = {
      singleProxy: oneProxy,
      proxyForHttp: httpProxy,
      socksProxy: socksProxy,
    };

    var config = { rules: rules };
    chrome.experimental.proxy.useCustomProxySettings(config);
    chrome.test.succeed();
  }
]);
