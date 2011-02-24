// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.ProxySystem

chrome.test.runTests([
  function setSystemProxy() {
    var config = { mode: "system" };
    chrome.experimental.proxy.settings.set(
        {'value': config},
        chrome.test.callbackPass());
  }
]);
