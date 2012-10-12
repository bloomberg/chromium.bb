// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  var getURL = chrome.extension.getURL;
  chrome.tabs.create({"url": "about:blank"}, function(tab) {
    var tabId = tab.id;

    chrome.test.runTests([
      // History manipulation via window.history
      function history() {
        expect([
          { label: "a-onBeforeNavigate",
            event: "onBeforeNavigate",
            details: { frameId: 0,
                       parentFrameId: -1,
                       processId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       url: getURL('history/a.html') }},
          { label: "a-onCommitted",
            event: "onCommitted",
            details: { frameId: 0,
                       processId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       transitionQualifiers: [],
                       transitionType: "link",
                       url: getURL('history/a.html') }},
          { label: "a-onDOMContentLoaded",
            event: "onDOMContentLoaded",
            details: { frameId: 0,
                       processId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       url: getURL('history/a.html') }},
          { label: "a-onCompleted",
            event: "onCompleted",
            details: { frameId: 0,
                       processId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       url: getURL('history/b.html') }},
          { label: "a-onHistoryStateUpdated",
            event: "onHistoryStateUpdated",
            details: { frameId: 0,
                       processId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       transitionQualifiers: [],
                       transitionType: "link",
                       url: getURL('history/b.html') }}],
          [ navigationOrder("a-") ]);
        chrome.tabs.update(tabId, { url: getURL('history/a.html') });
      },
    ]);
  });
};
