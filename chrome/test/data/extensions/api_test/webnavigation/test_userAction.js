// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function runTests() {
  var getURL = chrome.extension.getURL;
  chrome.tabs.create({"url": "about:blank"}, function(tab) {
    var tabId = tab.id;

    chrome.test.runTests([
      // Opens a tab and waits for the user to click on a link in it.
      function userAction() {
        expect([
          [ "onBeforeNavigate",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('userAction/a.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "typed",
              url: getURL('userAction/a.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('userAction/a.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('userAction/a.html') }],
          [ "onBeforeCreateNavigationTarget",
            { sourceFrameId: 0,
              sourceTabId: 0,
              tabId: 1,
              timeStamp: 0,
              url: getURL('userAction/b.html') }],
          [ "onBeforeNavigate",
            { frameId: 0,
              tabId: 1,
              timeStamp: 0,
              url: getURL('userAction/b.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 1,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "link",
              url: getURL('userAction/b.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 1,
              timeStamp: 0,
              url: getURL('userAction/b.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 1,
              timeStamp: 0,
              url: getURL('userAction/b.html') }]]);

        // Notify the api test that we're waiting for the user.
        chrome.test.notifyPass();
      },
    ]);
  });
}
