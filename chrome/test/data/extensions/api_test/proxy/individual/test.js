// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.ProxyFixedIndividual

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
    var httpsProxy = {
      host: "2.2.2.2"
    };
    var httpsProxyExpected = {
      scheme: "http",
      host: "2.2.2.2",
      port: 80
    };
    var ftpProxy = {
      host: "3.3.3.3",
      port: 9000
    };
    var ftpProxyExpected = {
      scheme: "http",  // this is added.
      host: "3.3.3.3",
      port: 9000
    };
    var fallbackProxy = {
      scheme: "socks4",
      host: "4.4.4.4",
      port: 9090
    };
    var fallbackProxyExpected = fallbackProxy;

    var rules = {
      proxyForHttp: httpProxy,
      proxyForHttps: httpsProxy,
      proxyForFtp: ftpProxy,
      fallbackProxy: fallbackProxy,
    };
    var rulesExpected = {
      proxyForHttp: httpProxyExpected,
      proxyForHttps: httpsProxyExpected,
      proxyForFtp: ftpProxyExpected,
      fallbackProxy: fallbackProxyExpected,
    };

    var config = { rules: rules, mode: "fixed_servers" };
    var configExpected = { rules: rulesExpected, mode: "fixed_servers" };

    chrome.experimental.proxy.settings.set(
        {'value': config},
        chrome.test.callbackPass());
    chrome.experimental.proxy.settings.get(
        {'incognito': false},
        expect({ 'value': configExpected,
                 'levelOfControl': "ControlledByThisExtension" },
               "invalid proxy settings"));
    chrome.experimental.proxy.settings.get(
        {'incognito': true},
        expect({ 'value': configExpected,
                 'levelOfControl': "ControlledByThisExtension" },
               "invalid proxy settings"));
   }
]);
