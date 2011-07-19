// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Idle api test for Chrome.
// browser_tests.exe --gtest_filter=ExtensionApiTest.Idle

// Due to the fact that browser tests are run in many different environments it
// is not simple to be able to set the user input value before testing.  For
// these bvts we have chosen the minimal consistent tests.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;

chrome.test.runTests([
  // Exercise querying the state.  Querying the state multiple times within
  // the same threshold exercises different code.
  function queryState() {
    chrome.idle.queryState(60, pass(function(state) {
      var previous_state = state;
      chrome.idle.queryState(120, pass(function(state) {
        assertEq(previous_state, state);
        chrome.test.succeed();
      }));
    }));
  },

  // Exercise the setting of the event listener.
  function setCallback() {
    chrome.idle.onStateChanged.addListener(function(state) {
      window.console.log('current state: ' + state);

      // The test has succeeded.
      chrome.test.succeed();
    });
  }
]);
