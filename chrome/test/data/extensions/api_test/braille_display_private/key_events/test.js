// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test for brailleDisplayPrivate.onKeyEvent.
// browser_tests.exe --gtest_filter="BrailleDisplayPrivateApiTest.*"

var pass = chrome.test.callbackPass;

var EXPECTED_EVENTS = [
  { "command": "line_up" },
  { "command": "line_down" },
]

var event_number = 0;
var allEventsReceived;

function eventListener(event) {
  console.log("Received event: " + JSON.stringify(event));
  chrome.test.assertEq(event, EXPECTED_EVENTS[event_number]);
  if (++event_number == EXPECTED_EVENTS.length) {
    allEventsReceived();
  }
  console.log("Event number: " + event_number);
}

function waitForDisplay(callback) {
  var callbackCompleted = chrome.test.callbackAdded();
  var displayStateHandler = function(state) {
    if (!callbackCompleted) {
      return;
    }
    chrome.test.assertTrue(state.available, "Display not available");
    chrome.test.assertEq(11, state.textCellCount);
    callback(state);
    callbackCompleted();
    chrome.brailleDisplayPrivate.onDisplayStateChanged.removeListener(
        displayStateHandler);
    // Prevent additional runs if the onDisplayStateChanged event
    // is fired before getDisplayState invokes the callback.
    callbackCompleted = null;
  };
  chrome.brailleDisplayPrivate.onDisplayStateChanged.addListener(
      displayStateHandler);
  chrome.brailleDisplayPrivate.getDisplayState(pass(function(state) {
    if (state.available) {
      displayStateHandler(state);
    } else {
      console.log("Display not ready yet");
    }
  }));
}

chrome.test.runTests([
  function testKeyEvents() {
    chrome.brailleDisplayPrivate.onKeyEvent.addListener(eventListener);
    allEventsReceived = chrome.test.callbackAdded();
    waitForDisplay(pass());
  }
]);
