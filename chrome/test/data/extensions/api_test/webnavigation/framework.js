// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var expectedEventData;
var capturedEventData;
var nextFrameId;
var frameIds;
var nextTabId;
var tabIds;
var initialized = false;

function expect(data) {
  expectedEventData = data;
  capturedEventData = [];
  nextFrameId = 1;
  frameIds = {};
  nextTabId = 0;
  tabIds = {};
  initListeners();
}

function checkExpectations() {
  if (capturedEventData.length < expectedEventData.length) {
    return;
  }
  chrome.test.assertEq(JSON.stringify(expectedEventData),
      JSON.stringify(capturedEventData));
  chrome.test.succeed();
}

function captureEvent(name, details) {
  // Skip about:blank navigations
  if ('url' in details && details.url == 'about:blank') {
    return;
  }
  // normalize details.
  if ('timeStamp' in details) {
    details.timeStamp = 0;
  }
  if (('frameId' in details) && (details.frameId != 0)) {
    if (frameIds[details.frameId] === undefined) {
      frameIds[details.frameId] = nextFrameId++;
    }
    details.frameId = frameIds[details.frameId];
  }
  if (('sourceFrameId' in details) && (details.sourceFrameId != 0)) {
    if (frameIds[details.sourceFrameId] === undefined) {
      frameIds[details.sourceFrameId] = nextFrameId++;
    }
    details.sourceFrameId = frameIds[details.sourceFrameId];
  }
  if ('tabId' in details) {
    if (tabIds[details.tabId] === undefined) {
      tabIds[details.tabId] = nextTabId++;
    }
    details.tabId = tabIds[details.tabId];
  }
  if ('sourceTabId' in details) {
    if (tabIds[details.sourceTabId] === undefined) {
      tabIds[details.sourceTabId] = nextTabId++;
    }
    details.sourceTabId = tabIds[details.sourceTabId];
  }
  capturedEventData.push([name, details]);
  checkExpectations();
}

function initListeners() {
  if (initialized)
    return;
  initialized = true;
  chrome.experimental.webNavigation.onBeforeNavigate.addListener(
      function(details) {
    captureEvent("onBeforeNavigate", details);
  });
  chrome.experimental.webNavigation.onCommitted.addListener(
      function(details) {
    captureEvent("onCommitted", details);
  });
  chrome.experimental.webNavigation.onDOMContentLoaded.addListener(
      function(details) {
    captureEvent("onDOMContentLoaded", details);
  });
  chrome.experimental.webNavigation.onCompleted.addListener(
      function(details) {
    captureEvent("onCompleted", details);
  });
  chrome.experimental.webNavigation.onBeforeCreateNavigationTarget.addListener(
      function(details) {
    captureEvent("onBeforeCreateNavigationTarget", details);
  });
  chrome.experimental.webNavigation.onReferenceFragmentUpdated.addListener(
      function(details) {
    captureEvent("onReferenceFragmentUpdated", details);
  });
  chrome.experimental.webNavigation.onErrorOccurred.addListener(
      function(details) {
    captureEvent("onErrorOccurred", details);
  });
}
