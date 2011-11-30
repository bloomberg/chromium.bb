// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tab where the content script has been injected.
var testTabId;

chrome.test.getConfig(function(config) {

  function rewriteURL(url) {
    return url.replace(/PORT/, config.testServer.port);
  }

  function doReq(domain, expectSuccess) {
    var url = rewriteURL(domain + ':PORT/files/extensions/test_file.txt');

    chrome.tabs.sendRequest(testTabId, url, function(response) {
      if (expectSuccess) {
        chrome.test.assertEq('load', response.event);
        chrome.test.assertEq(200, response.status);
        chrome.test.assertEq('Hello!', response.text);
      } else {
        chrome.test.assertEq(0, response.status);
      }

      chrome.test.runNextTest();
    });
  }

  chrome.tabs.create({
      url: rewriteURL('http://localhost:PORT/files/extensions/test_file.html')},
      function(tab) {
        testTabId = tab.id;
      });

  chrome.extension.onRequest.addListener(function(message) {
    chrome.test.assertEq('injected', message);

    chrome.test.runTests([
      function allowedOrigin() {
        doReq('http://a.com', true);
      },
      function diallowedOrigin() {
        doReq('http://c.com', false);
      },
      function allowedSubdomain() {
        doReq('http://foo.b.com', true);
      },
      function noSubdomain() {
        doReq('http://b.com', true);
      },
      function disallowedSubdomain() {
        doReq('http://foob.com', false);
      },
      function disallowedSSL() {
        doReq('https://a.com', false);
      },
      function targetPageAlwaysAllowed() {
        // Even though localhost does not show up in the host permissions, we
        // can still make requests to it since it's the page that the content
        // script is injected into.
        doReq('http://localhost', true);
      }
    ]);
  });
});
