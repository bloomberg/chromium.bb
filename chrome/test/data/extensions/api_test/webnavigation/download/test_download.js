// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  var getURL = chrome.extension.getURL;
  var URL_LOAD_REDIRECT = "http://127.0.0.1:PORT/server-redirect";
  var URL_NOT_FOUND = "http://127.0.0.1:PORT/not-found";
  chrome.tabs.create({"url": "about:blank"}, function(tab) {
    var tabId = tab.id;
    chrome.test.getConfig(function(config) {
      var fixPort = function(url) {
        return url.replace(/PORT/g, config.testServer.port);
      };
      URL_LOAD_REDIRECT = fixPort(URL_LOAD_REDIRECT);
      URL_NOT_FOUND = fixPort(URL_NOT_FOUND);
      chrome.test.runTests([
        // Navigates to a page that redirects (on the server side) to a.html.
        function serverRedirect() {
          expect([
            { label: "a-onBeforeNavigate",
              event: "onBeforeNavigate",
              details: { frameId: 0,
                         parentFrameId: -1,
                         processId: -1,
                         tabId: 0,
                         timeStamp: 0,
                         url: getURL('a.html') }},
            { label: "a-onCommitted",
              event: "onCommitted",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         transitionQualifiers: [],
                         transitionType: "link",
                         url: getURL('a.html') }},
            { label: "a-onDOMContentLoaded",
              event: "onDOMContentLoaded",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: getURL('a.html') }},
            { label: "a-onCompleted",
              event: "onCompleted",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: getURL('a.html') }},
            { label: "b-onBeforeNavigate",
              event: "onBeforeNavigate",
              details: { frameId: 0,
                         parentFrameId: -1,
                         processId: -1,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_LOAD_REDIRECT }},
            { label: "b-onCommitted",
              event: "onCommitted",
              details: { frameId: 0,
                         processId: 1,
                         tabId: 0,
                         timeStamp: 0,
                         transitionQualifiers: ["server_redirect"],
                         transitionType: "link",
                         url: URL_NOT_FOUND }},
            { label: "b-onDOMContentLoaded",
              event: "onDOMContentLoaded",
              details: { frameId: 0,
                         processId: 1,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_NOT_FOUND }},
            { label: "b-onCompleted",
              event: "onCompleted",
              details: { frameId: 0,
                         processId: 1,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_NOT_FOUND }},
            { label: "c-onBeforeNavigate",
              event: "onBeforeNavigate",
              details: { frameId: 0,
                         parentFrameId: -1,
                         processId: -1,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_NOT_FOUND }},
            { label: "c-onErrorOccurred",
              event: "onErrorOccurred",
              details: { error: "net::ERR_ABORTED",
                         frameId: 0,
                         processId: -1,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_NOT_FOUND }},
            { label: "d-onBeforeNavigate",
              event: "onBeforeNavigate",
              details: { frameId: 0,
                         parentFrameId: -1,
                         processId: -1,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_NOT_FOUND }},
            { label: "d-onErrorOccurred",
              event: "onErrorOccurred",
              details: { error: "net::ERR_ABORTED",
                         frameId: 0,
                         processId: -1,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_NOT_FOUND }}],
          [ navigationOrder("a-"), navigationOrder("b-") ]);
          chrome.tabs.update(
              tabId, { url: getURL('a.html?' + config.testServer.port) });
        },
      ]);
    });
  });
};
