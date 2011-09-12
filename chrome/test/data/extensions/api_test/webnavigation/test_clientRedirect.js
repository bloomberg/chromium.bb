// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function runTests() {
  var getURL = chrome.extension.getURL;
  chrome.tabs.create({"url": "about:blank"}, function(tab) {
    var tabId = tab.id;
    chrome.test.runTests([
      // Navigates to a.html that redirects to b.html (using javascript).
      function clientRedirect() {
        expect([
          { label: "a-onBeforeNavigate",
            event: "onBeforeNavigate",
            details: { frameId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       url: getURL('clientRedirect/a.html') }},
          { label: "a-onCommitted",
            event: "onCommitted",
            details: { frameId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       transitionQualifiers: [],
                       transitionType: "link",
                       url: getURL('clientRedirect/a.html') }},
          { label: "a-onDOMContentLoaded",
            event: "onDOMContentLoaded",
            details: { frameId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       url: getURL('clientRedirect/a.html') }},
          { label: "a-onCompleted",
            event: "onCompleted",
            details: { frameId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       url: getURL('clientRedirect/a.html') }},
          { label: "b-onBeforeNavigate",
            event: "onBeforeNavigate",
            details: { frameId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       url: getURL('clientRedirect/b.html') }},
          { label: "b-onCommitted",
            event: "onCommitted",
            details: { frameId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       transitionQualifiers: ["client_redirect"],
                       transitionType: "link",
                       url: getURL('clientRedirect/b.html') }},
          { label: "b-onDOMContentLoaded",
            event: "onDOMContentLoaded",
            details: { frameId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       url: getURL('clientRedirect/b.html') }},
          { label: "b-onCompleted",
            event: "onCompleted",
            details: { frameId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       url: getURL('clientRedirect/b.html') }}],
          [ navigationOrder("a-"),
            navigationOrder("b-"),
            isLoadedBy("b-", "a-") ]);
        chrome.tabs.update(tabId, { url: getURL('clientRedirect/a.html') });
      },
    ]);
  });
}
