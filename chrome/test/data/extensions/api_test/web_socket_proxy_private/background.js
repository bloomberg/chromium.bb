// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var hostname = '127.0.0.1';
var port = -1;

function testModernApi() {
  function gotMessage(msg) {
    chrome.test.assertEq(msg.data, window.btoa("aloha\n"));
  }

  function gotUrl(url) {
    var url_regex = /^ws:\/\//
    chrome.test.assertEq(0, url.search(url_regex));
    ws = new WebSocket(url);

    function gotMessage(msg) {
      // chrome.test.callbackPass envelope works only once, remove it.
      ws.onmessage = gotMessage;
    }

    // We should get response from HTTP test server.
    ws.onmessage = chrome.test.callbackPass(gotMessage);

    ws.onopen = function() {
      var request = ["GET / HTTP/1.1",
                     "Host: 127.0.0.1:" + port,
                     "Connection: keep-alive",
                     "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/535.2 (KHTML, like Gecko) Chrome/15.0.874.54 Safari/535.2",
                     "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
                     "", ""].join("\r\n");
      ws.send(window.btoa(request));
    };
  }

  chrome.webSocketProxyPrivate.getURLForTCP(
      hostname, port, { "tls": false }, chrome.test.callbackPass(gotUrl));
}

function testObsoleteApi() {
  function gotPassport(passport) {
    chrome.test.assertEq('string', typeof(passport));
    chrome.test.assertTrue(passport.length > 5);  // Short one is insecure.
  }

  chrome.webSocketProxyPrivate.getPassportForTCP(
      hostname, port, chrome.test.callbackPass(gotPassport));
}

function setTestServerPortAndProceed(testConfig) {
  port = testConfig.testServer.port;
  chrome.test.runTests([testModernApi, testObsoleteApi]);
}

chrome.test.getConfig(setTestServerPortAndProceed);
