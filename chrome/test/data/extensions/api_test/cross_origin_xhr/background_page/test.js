// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {

  function doReq(domain, expectSuccess) {
    var req = new XMLHttpRequest();
    var url = domain + ":PORT/files/extensions/test_file.txt";
    url = url.replace(/PORT/, config.testServer.port);

    chrome.test.log("Requesting url: " + url);
    req.open("GET", url, true);


    if (expectSuccess) {
      req.onload = function() {
        chrome.test.assertEq(200, req.status);
        chrome.test.assertEq("Hello!", req.responseText);
        chrome.test.runNextTest();
      }
      req.onerror = function() {
        chrome.test.log("status: " + req.status);
        chrome.test.log("text: " + req.responseText);
        chrome.test.fail("Unexpected error for domain: " + domain);
      }
    } else {
      req.onload = function() {
        chrome.test.fail("Unexpected success for domain: " + domain);
      }
      req.onerror = function() {
        chrome.test.assertEq(0, req.status);
        chrome.test.runNextTest();
      }
    }

    req.send(null);
  }

  chrome.test.runTests([
    function allowedOrigin() {
      doReq("http://a.com", true);
    },
    function diallowedOrigin() {
      doReq("http://c.com", false);
    },
    function allowedSubdomain() {
      doReq("http://foo.b.com", true);
    },
    function noSubdomain() {
      doReq("http://b.com", true);
    },
    function disallowedSubdomain() {
      doReq("http://foob.com", false);
    },
    function disallowedSSL() {
      doReq("https://a.com", false);
    }
  ]);
});
