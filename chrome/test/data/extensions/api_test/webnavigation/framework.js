// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var deepEq = chrome.test.checkDeepEq;
var expectedEventData;
var expectedEventOrder;
var capturedEventData;
var nextFrameId;
var frameIds;
var nextTabId;
var tabIds;
var initialized = false;

// data: array of expected events, each one is a dictionary:
//     { label: "<unique identifier>",
//       event: "<webnavigation event type>",
//       details: { <expected details of the event> }
//     }
// order: an array of sequences, e.g. [ ["a", "b", "c"], ["d", "e"] ] means that
//     event with label "a" needs to occur before event with label "b". The
//     relative order of "a" and "d" does not matter.
function expect(data, order) {
  expectedEventData = data;
  capturedEventData = [];
  expectedEventOrder = order;
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
  if (capturedEventData.length > expectedEventData.length) {
    chrome.test.fail("Recorded too many events. " +
        JSON.stringify(capturedEventData));
  }
  // We have ensured that capturedEventData contains exactly the same elements
  // as expectedEventData. Now we need to verify the ordering.
  // Step 1: build positions such that
  //     position[<event-label>]=<position of this event in capturedEventData>
  var curPos = 0;
  var positions = {};
  capturedEventData.forEach(function (event) {
    chrome.test.assertTrue(event.hasOwnProperty("label"));
    positions[event.label] = curPos;
    curPos++;
  });
  // Step 2: check that elements arrived in correct order
  expectedEventOrder.forEach(function (order) {
    var previousLabel = undefined;
    order.forEach(function (label) {
      if (previousLabel === undefined) {
        previousLabel = label;
        return;
      }
      chrome.test.assertTrue(positions[previousLabel] < positions[label],
          "Event " + previousLabel + " is supposed to arrive before " +
          label + ".");
      previousLabel = label;
    });
  });
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

  // find |details| in expectedEventData
  var found = false;
  var label = undefined;
  expectedEventData.forEach(function (exp) {
    if (deepEq(exp.event, name) && deepEq(exp.details, details)) {
      if (!found) {
        found = true;
        label = exp.label;
        exp.event = undefined;
      }
    }
  });
  if (!found) {
    chrome.test.fail("Received unexpected event '" + name + "':" +
        JSON.stringify(details));
  }
  capturedEventData.push({label: label, event: name, details: details});
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
  chrome.experimental.webNavigation.onCreatedNavigationTarget.addListener(
      function(details) {
    captureEvent("onCreatedNavigationTarget", details);
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

// Returns the usual order of navigation events.
function navigationOrder(prefix) {
  return [ prefix + "onBeforeNavigate",
           prefix + "onCommitted",
           prefix + "onDOMContentLoaded",
           prefix + "onCompleted" ];
}

// Returns the constraints expressing that a frame is an iframe of another
// frame.
function isIFrameOf(iframe, main_frame) {
  return [ main_frame + "onCommitted",
           iframe + "onBeforeNavigate",
           main_frame + "onDOMContentLoaded",
           iframe + "onCompleted",
           main_frame + "onCompleted" ];
}

// Returns the constraint expressing that a frame was loaded by another.
function isLoadedBy(target, source) {
  return [ source + "onDOMContentLoaded", target + "onBeforeNavigate"];
}
