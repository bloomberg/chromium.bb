// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function runTests() {
  var getURL = chrome.extension.getURL;
  chrome.tabs.create({"url": "about:blank"}, function(tab) {
    var tabId = tab.id;

    chrome.test.runTests([
      // Navigates to a non-existant page.
      function nonExistant() {
        expect([
          [ "onBeforeNavigate",
            { frameId: 0,
              requestId: "0",
              tabId: 0,
              timeStamp: 0,
              url: getURL('failures/nonexistant.html') }],
          [ "onErrorOccurred",
            { error: "net::ERR_FILE_NOT_FOUND",
              frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('failures/nonexistant.html') }]]);
        chrome.tabs.update(tabId, { url: getURL('failures/nonexistant.html') });
      },

      // An page that tries to load an non-existant iframe.
      function nonExistantIframe() {
        expect([
          [ "onBeforeNavigate",
            { frameId: 0,
              requestId: "0",
              tabId: 0,
              timeStamp: 0,
              url: getURL('failures/d.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "link",
              url: getURL('failures/d.html') }],
          [ "onBeforeNavigate",
            { frameId: 1,
              requestId: "0",
              tabId: 0,
              timeStamp: 0,
              url: getURL('failures/c.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('failures/d.html') }],
          [ "onErrorOccurred",
            { error: "net::ERR_FILE_NOT_FOUND",
              frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('failures/c.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('failures/d.html') }]]);
        chrome.tabs.update(tabId, { url: getURL('failures/d.html') });
      },

      // An iframe navigates to a non-existant page.
      function nonExistantIframeNavigation() {
        expect([
          [ "onBeforeNavigate",
            { frameId: 0,
              requestId: "0",
              tabId: 0,
              timeStamp: 0,
              url: getURL('failures/a.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "link",
              url: getURL('failures/a.html') }],
          [ "onBeforeNavigate",
            { frameId: 1,
              requestId: "0",
              tabId: 0,
              timeStamp: 0,
              url: getURL('failures/b.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('failures/a.html') }],
          [ "onCommitted",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "auto_subframe",
              url: getURL('failures/b.html') }],
          [ "onDOMContentLoaded",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('failures/b.html') }],
          [ "onCompleted",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('failures/b.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('failures/a.html') }],
          [ "onBeforeNavigate",
            { frameId: 1,
              requestId: "0",
              tabId: 0,
              timeStamp: 0,
              url: getURL('failures/c.html') }],
          [ "onErrorOccurred",
            { error: "net::ERR_FILE_NOT_FOUND",
              frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('failures/c.html') }]]);
        chrome.tabs.update(tabId, { url: getURL('failures/a.html') });
      },
    ]);
  });
}
