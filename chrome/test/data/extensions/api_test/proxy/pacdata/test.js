// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.ProxyPacData

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

chrome.test.runTests([
  function setAutoSettings() {
    var pacScriptObject = {
      data: "function FindProxyForURL(url, host) {\n" +
            "  if (host == 'foobar.com')\n" +
            "    return 'PROXY blackhole:80';\n" +
            "  return 'DIRECT';\n" +
            "}"
    };
    var config = {
      mode: "pac_script",
      pacScript: pacScriptObject
    };
    chrome.experimental.proxy.useCustomProxySettings(config);
    chrome.experimental.proxy.getCurrentProxySettings(
        false,
        expect(config, "invalid proxy settings"));
  }
]);
