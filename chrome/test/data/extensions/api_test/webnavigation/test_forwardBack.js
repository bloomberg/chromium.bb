// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function runTests() {
  var getURL = chrome.extension.getURL;
  chrome.tabs.create({"url": "about:blank"}, function(tab) {
    var tabId = tab.id;

    chrome.test.runTests([
      // First navigates to a.html which redirects to to b.html which uses
      // history.back() to navigate back to a.html
      function forwardBack() {
        expect([
          [ "onBeforeNavigate",
            { frameId: 0,
              requestId: "0",
              tabId: 0,
              timeStamp: 0,
              url: getURL('forwardBack/a.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "link",
              url: getURL('forwardBack/a.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('forwardBack/a.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('forwardBack/a.html') }],
          [ "onBeforeNavigate",
            { frameId: 0,
              requestId: "0",
              tabId: 0,
              timeStamp: 0,
              url: getURL('forwardBack/b.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: ["client_redirect"],
              transitionType: "link",
              url: getURL('forwardBack/b.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('forwardBack/b.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('forwardBack/b.html') }],
          [ "onBeforeNavigate",
            { frameId: 0,
              requestId: "0",
              tabId: 0,
              timeStamp: 0,
              url: getURL('forwardBack/a.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: ["forward_back"],
              transitionType: "link",
              url: getURL('forwardBack/a.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('forwardBack/a.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('forwardBack/a.html') }]]);
        chrome.tabs.update(tabId, { url: getURL('forwardBack/a.html') });
      },
    ]);
  });
}
