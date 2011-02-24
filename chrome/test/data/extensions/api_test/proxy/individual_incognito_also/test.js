// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe
//     --gtest_filter=ExtensionApiTest.ProxyFixedIndividualIncognitoAlso

function setIndividualProxiesRegular() {
  var httpProxy = {
    host: "1.1.1.1"
  };
  var httpsProxy = {
    scheme: "socks5",
    host: "2.2.2.2"
  };
  var ftpProxy = {
    host: "3.3.3.3",
    port: 9000
  };
  var fallbackProxy = {
    scheme: "socks4",
    host: "4.4.4.4",
    port: 9090
  };

  var rules = {
    proxyForHttp: httpProxy,
    proxyForHttps: httpsProxy,
    proxyForFtp: ftpProxy,
    fallbackProxy: fallbackProxy,
  };

  var config = { rules: rules, mode: "fixed_servers" };
  chrome.experimental.proxy.settings.set(
      {'value': config, 'incognito': false},
      chrome.test.callbackPass());
}

function setIndividualProxiesIncognito() {
  var httpProxy = {
    host: "5.5.5.5"
  };
  var httpsProxy = {
    scheme: "socks5",
    host: "6.6.6.6"
  };
  var ftpProxy = {
    host: "7.7.7.7",
    port: 9000
  };
  var fallbackProxy = {
    scheme: "socks4",
    host: "8.8.8.8",
    port: 9090
  };

  var rules = {
    proxyForHttp: httpProxy,
    proxyForHttps: httpsProxy,
    proxyForFtp: ftpProxy,
    fallbackProxy: fallbackProxy,
  };

  var config = { rules: rules, mode: "fixed_servers" };
  chrome.experimental.proxy.settings.set(
      {'value': config, 'incognito': true},
      chrome.test.callbackPass());
}

chrome.test.runTests([
  function setIndividualProxies() {
    setIndividualProxiesRegular();
    setIndividualProxiesIncognito();
  }
]);
