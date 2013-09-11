// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test for brailleDisplayPrivate.onKeyEvent.
// browser_tests.exe --gtest_filter="BrailleDisplayPrivateApiTest.*"

var pass = chrome.test.callbackPass;

function createBuffer(size, element) {
  var buf = new Uint8Array(size);
  for (var i = 0; i < size; ++i) {
    buf[i] = element;
  }
  return buf.buffer;
}

var EXPECTED_EVENTS = [
  { "command": "line_up" },
  { "command": "line_down" },
]

var event_number = 0;
var callbackCompleted;

function eventListener(event) {
  console.log("Received event: " + event);
  chrome.test.assertEq(event, EXPECTED_EVENTS[event_number]);
  if (++event_number == EXPECTED_EVENTS.length) {
    console.log("Happy!");
    callbackCompleted();
  }
  console.log("Event number: " + event_number);
}

chrome.test.runTests([
  function testKeyEvents() {
    chrome.brailleDisplayPrivate.onKeyEvent.addListener(eventListener);
    callbackCompleted = chrome.test.callbackAdded();
    chrome.brailleDisplayPrivate.getDisplayState(pass(
        function(state) {
          chrome.test.assertTrue(state.available, "Display not available");
          chrome.test.assertEq(11, state.textCellCount);
        }));
  }
]);
