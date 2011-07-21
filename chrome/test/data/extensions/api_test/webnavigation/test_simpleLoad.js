// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function runTests() {
  var getURL = chrome.extension.getURL;
  chrome.tabs.create({"url": "about:blank"}, function(tab) {
    var tabId = tab.id;

    chrome.test.runTests([
      // Navigates to an URL.
      function simpleLoad() {
        expect([
          [ "onBeforeNavigate",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('simpleLoad/a.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "link",
              url: getURL('simpleLoad/a.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('simpleLoad/a.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('simpleLoad/a.html') }]]);
        chrome.tabs.update(tabId, { url: getURL('simpleLoad/a.html') });
      },
    ]);
  });
}
