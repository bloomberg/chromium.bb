// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function runTests() {
  var getURL = chrome.extension.getURL;
  chrome.tabs.create({"url": "about:blank"}, function(tab) {
    var tabId = tab.id;
    chrome.test.runTests([
      // Navigates to a.html that redirects to b.html (using javascript)
      // after a delay of 500ms, so the initial navigation is completed and
      // the redirection is marked as client_redirect.
      function clientRedirect() {
        expect([
          [ "onBeforeNavigate",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('clientRedirect/a.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "link",
              url: getURL('clientRedirect/a.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('clientRedirect/a.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('clientRedirect/a.html') }],
          [ "onBeforeNavigate",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('clientRedirect/b.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: ["client_redirect"],
              transitionType: "link",
              url: getURL('clientRedirect/b.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('clientRedirect/b.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('clientRedirect/b.html') }]]);
        chrome.tabs.update(tabId, { url: getURL('clientRedirect/a.html') });
      },
    ]);
  });
}
