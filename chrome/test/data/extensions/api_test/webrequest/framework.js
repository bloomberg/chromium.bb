// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var getURL = chrome.extension.getURL;
var deepEq = chrome.test.checkDeepEq;
var expectedEventData;
var capturedEventData;
var expectedEventOrder;
var tabId;
var tabIdMap;
var frameIdMap;
var testServerPort;
var testServer = "www.a.com";
var eventsCaptured;

function runTests(tests) {
  chrome.tabs.create({url: "about:blank"}, function(tab) {
    tabId = tab.id;
    tabIdMap = {};
    tabIdMap[tabId] = 0;
    chrome.test.getConfig(function(config) {
      testServerPort = config.testServer.port;
      chrome.test.runTests(tests);
    });
  });
}

// Returns an URL from the test server, fixing up the port. Must be called
// from within a test case passed to runTests.
function getServerURL(path, opt_host) {
  if (!testServerPort)
    throw new Error("Called getServerURL outside of runTests.");
  var host = opt_host || testServer;
  return "http://" + host + ":" + testServerPort + "/" + path;
}

// Helper to advance to the next test only when the tab has finished loading.
// This is because tabs.update can sometimes fail if the tab is in the middle
// of a navigation (from the previous test), resulting in flakiness.
function navigateAndWait(url, callback) {
  var done = chrome.test.listenForever(chrome.tabs.onUpdated,
      function (_, info, tab) {
    if (tab.id == tabId && info.status == "complete") {
      if (callback) callback();
      done();
    }
  });
  chrome.tabs.update(tabId, {url: url});
}

// data: array of extected events, each one is a dictionary:
//     { label: "<unique identifier>",
//       event: "<webrequest event type>",
//       details: { <expected details of the webrequest event> },
//       retval: { <dictionary that the event handler shall return> } (optional)
//     }
// order: an array of sequences, e.g. [ ["a", "b", "c"], ["d", "e"] ] means that
//     event with label "a" needs to occur before event with label "b". The
//     relative order of "a" and "d" does not matter.
// filter: filter dictionary passed on to the event subscription of the
//     webRequest API.
// extraInfoSpec: the union of all desired extraInfoSpecs for the events.
function expect(data, order, filter, extraInfoSpec) {
  expectedEventData = data;
  capturedEventData = [];
  expectedEventOrder = order;
  eventsCaptured = chrome.test.callbackAdded();
  tabAndFrameUrls = {};  // Maps "{tabId}-{frameId}" to the URL of the frame.
  frameIdMap = {"-1": -1};
  removeListeners();
  initListeners(filter || {}, extraInfoSpec || []);
  // Fill in default values.
  for (var i = 0; i < expectedEventData.length; ++i) {
    if (!('method' in expectedEventData[i].details)) {
      expectedEventData[i].details.method = "GET";
    }
    if (!('tabId' in expectedEventData[i].details)) {
      expectedEventData[i].details.tabId = tabIdMap[tabId];
    }
    if (!('frameId' in expectedEventData[i].details)) {
      expectedEventData[i].details.frameId = 0;
    }
    if (!('parentFrameId' in expectedEventData[i].details)) {
      expectedEventData[i].details.parentFrameId = -1;
    }
    if (!('type' in expectedEventData[i].details)) {
      expectedEventData[i].details.type = "main_frame";
    }
  }
}

function checkExpectations() {
  if (capturedEventData.length < expectedEventData.length) {
    return;
  }
  if (capturedEventData.length > expectedEventData.length) {
    chrome.test.fail("Recorded too many events. " +
        JSON.stringify(capturedEventData));
    return;
  }
  // We have ensured that capturedEventData contains exactly the same elements
  // as expectedEventData. Now we need to verify the ordering.
  // Step 1: build positions such that
  //     positions[<event-label>]=<position of this event in capturedEventData>
  var curPos = 0;
  var positions = {}
  capturedEventData.forEach(function (event) {
    chrome.test.assertTrue(event.hasOwnProperty("label"));
    positions[event.label] = curPos;
    curPos++;
  });
  // Step 2: check that elements arrived in correct order
  expectedEventOrder.forEach(function (order) {
    var previousLabel = undefined;
    order.forEach(function(label) {
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

  eventsCaptured();
}

// Simple check to see that we have a User-Agent header, and that it contains
// an expected value. This is a basic check that the request headers are valid.
function checkUserAgent(headers) {
  for (var i in headers) {
    if (headers[i].name.toLowerCase() == "user-agent")
      return headers[i].value.toLowerCase().indexOf("chrome") != -1;
  }
  return false;
}

function captureEvent(name, details, callback) {
  // Ignore system-level requests like safebrowsing updates and favicon fetches
  // since they are unpredictable.
  if (details.tabId == -1 || details.type == "other" ||
      details.url.match(/\/favicon.ico$/) ||
      details.url.match(/https:\/\/dl.google.com/))
    return;

  // Pull the extra per-event options out of the expected data. These let
  // us specify special return values per event.
  var currentIndex = capturedEventData.length;
  var extraOptions;
  if (expectedEventData.length > currentIndex) {
    retval =
        expectedEventData[currentIndex].retval_function ?
        expectedEventData[currentIndex].retval_function(name, details) :
        expectedEventData[currentIndex].retval;
  }

  // Check that the frameId can be used to reliably determine the URL of the
  // frame that caused requests.
  if (name == "onBeforeRequest") {
    chrome.test.assertTrue('frameId' in details &&
                           typeof details.frameId === 'number');
    chrome.test.assertTrue('tabId' in details &&
                            typeof details.tabId === 'number');
    var key = details.tabId + "-" + details.frameId;
    if (details.type == "main_frame" || details.type == "sub_frame") {
      tabAndFrameUrls[key] = details.url;
    }
    details.frameUrl = tabAndFrameUrls[key] || "unknown frame URL";
  }

  // This assigns unique IDs to frames. The new IDs are only deterministic, if
  // the frames documents are loaded in order. Don't write browser tests with
  // more than one frame ID and rely on their numbers.
  if (!(details.frameId in frameIdMap)) {
    // Subtract one to discount for {"-1": -1} mapping that always exists.
    // This gives the first frame the ID 0.
    frameIdMap[details.frameId] = Object.keys(frameIdMap).length - 1;
  }
  details.frameId = frameIdMap[details.frameId];
  details.parentFrameId = frameIdMap[details.parentFrameId];

  // This assigns unique IDs to newly opened tabs. However, the new IDs are only
  // deterministic, if the order in which the tabs are opened is deterministic.
  if (!(details.tabId in tabIdMap)) {
    tabIdMap[details.tabId] = Object.keys(tabIdMap).length;
  }
  details.tabId = tabIdMap[details.tabId];

  delete details.requestId;
  delete details.timeStamp;
  if (details.requestHeaders) {
    details.requestHeadersValid = checkUserAgent(details.requestHeaders);
    delete details.requestHeaders;
  }
  if (details.responseHeaders) {
    details.responseHeadersExist = true;
    delete details.responseHeaders;
  }

  // find |details| in expectedEventData
  var found = false;
  var label = undefined;
  expectedEventData.forEach(function (exp) {
    if (deepEq(exp.event, name) && deepEq(exp.details, details)) {
      if (found) {
        chrome.test.fail("Received event twice '" + name + "':" +
            JSON.stringify(details));
      } else {
        found = true;
        label = exp.label;
      }
    }
  });
  if (!found) {
    console.log("Expected events: " +
        JSON.stringify(expectedEventData, null, 2));
    chrome.test.fail("Received unexpected event '" + name + "':" +
        JSON.stringify(details, null, 2));
  }

  capturedEventData.push({label: label, event: name, details: details});
  checkExpectations();

  if (callback) {
    window.setTimeout(callback, 0, retval);
  } else {
    return retval;
  }
}

// Simple array intersection. We use this to filter extraInfoSpec so
// that only the allowed specs are sent to each listener.
function intersect(array1, array2) {
  return array1.filter(function(x) { return array2.indexOf(x) != -1; });
}

function initListeners(filter, extraInfoSpec) {
  chrome.webRequest.onBeforeRequest.addListener(
      function(details) {
    return captureEvent("onBeforeRequest", details);
  }, filter, intersect(extraInfoSpec, ["blocking"]));
  chrome.webRequest.onBeforeSendHeaders.addListener(
      function(details) {
    return captureEvent("onBeforeSendHeaders", details);
  }, filter, intersect(extraInfoSpec, ["blocking", "requestHeaders"]));
  chrome.webRequest.onSendHeaders.addListener(
      function(details) {
    return captureEvent("onSendHeaders", details);
  }, filter, intersect(extraInfoSpec, ["requestHeaders"]));
  chrome.webRequest.onHeadersReceived.addListener(
      function(details) {
    return captureEvent("onHeadersReceived", details);
  }, filter, intersect(extraInfoSpec, ["blocking", "responseHeaders"]));
  chrome.webRequest.onAuthRequired.addListener(
      function(details, callback) {
    return captureEvent("onAuthRequired", details, callback);
  }, filter, intersect(extraInfoSpec, ["asyncBlocking", "blocking",
                                       "responseHeaders"]));
  chrome.webRequest.onResponseStarted.addListener(
      function(details) {
    return captureEvent("onResponseStarted", details);
  }, filter, intersect(extraInfoSpec, ["responseHeaders"]));
  chrome.webRequest.onBeforeRedirect.addListener(
      function(details) {
    return captureEvent("onBeforeRedirect", details);
  }, filter, intersect(extraInfoSpec, ["responseHeaders"]));
  chrome.webRequest.onCompleted.addListener(
      function(details) {
    return captureEvent("onCompleted", details);
  }, filter, intersect(extraInfoSpec, ["responseHeaders"]));
  chrome.webRequest.onErrorOccurred.addListener(
      function(details) {
    return captureEvent("onErrorOccurred", details);
  }, filter);
}

function removeListeners() {
  function helper(event) {
    // Note: We're poking at the internal event data, but it's easier than
    // the alternative. If this starts failing, we just need to update this
    // helper.
    for (var i in event.subEvents_) {
      event.removeListener(event.subEvents_[i].callback);
    }
    chrome.test.assertFalse(event.hasListeners());
  }
  helper(chrome.webRequest.onBeforeRequest);
  helper(chrome.webRequest.onBeforeSendHeaders);
  helper(chrome.webRequest.onAuthRequired);
  helper(chrome.webRequest.onSendHeaders);
  helper(chrome.webRequest.onHeadersReceived);
  helper(chrome.webRequest.onResponseStarted);
  helper(chrome.webRequest.onBeforeRedirect);
  helper(chrome.webRequest.onCompleted);
  helper(chrome.webRequest.onErrorOccurred);
}
