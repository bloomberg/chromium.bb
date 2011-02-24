// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.ProxyFixedSingle

chrome.test.runTests([
  function setSingleProxy() {
    var oneProxy = {
      host: "127.0.0.1",
      port: 100
    };

    var rules = {
      singleProxy: oneProxy
    };

    var config = { rules: rules, mode: "fixed_servers" };
    chrome.experimental.proxy.settings.set(
        {'value': config},
        chrome.test.callbackPass());
  }
]);
