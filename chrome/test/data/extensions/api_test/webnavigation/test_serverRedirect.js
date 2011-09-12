// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function runTests() {
  var getURL = chrome.extension.getURL;
  var URL_LOAD =
      "http://www.a.com:PORT/files/extensions/api_test/webnavigation/serverRedirect/a.html";
  var URL_LOAD_REDIRECT = "http://www.a.com:PORT/server-redirect?" + URL_LOAD;
  chrome.tabs.create({"url": "about:blank"}, function(tab) {
    var tabId = tab.id;
    chrome.test.getConfig(function(config) {
      var fixPort = function(url) {
        return url.replace(/PORT/g, config.testServer.port);
      };
      URL_LOAD_REDIRECT = fixPort(URL_LOAD_REDIRECT);
      URL_LOAD = fixPort(URL_LOAD);
      chrome.test.runTests([
        // Navigates to a page that redirects (on the server side) to a.html.
        function serverRedirect() {
          expect([
            { label: "a-onBeforeNavigate",
              event: "onBeforeNavigate",
              details: { frameId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_LOAD_REDIRECT }},
            { label: "a-onCommitted",
              event: "onCommitted",
              details: { frameId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         transitionQualifiers: [],
                         transitionType: "link",
                         url: URL_LOAD }},
            { label: "a-onDOMContentLoaded",
              event: "onDOMContentLoaded",
              details: { frameId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_LOAD }},
            { label: "a-onCompleted",
              event: "onCompleted",
              details: { frameId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_LOAD }}],
            [ navigationOrder("a-") ]);
          chrome.tabs.update(tabId, { url: URL_LOAD_REDIRECT });
        },
      ]);
    });
  });
}
