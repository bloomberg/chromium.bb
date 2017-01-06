// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (!chrome || !chrome.test)
  throw new Error('chrome.test is undefined');

// This is a good end-to-end test for two reasons. The first is obvious - it
// tests a simple API and makes sure it behaves as expected, as well as testing
// that other APIs are unavailable.
// The second is that chrome.test is itself an extension API, and a rather
// complex one. It requires both traditional bindings (renderer parses args,
// passes info to browser process, browser process does work and responds, re-
// enters JS) and custom JS bindings (in order to have our runTests, assert*
// methods, etc). If any of these stages failed, the test itself would also
// fail.
chrome.test.runTests([
  function idleApi() {
    chrome.test.assertTrue(!!chrome.idle);
    chrome.test.assertTrue(!!chrome.idle.IdleState);
    chrome.test.assertTrue(!!chrome.idle.IdleState.IDLE);
    chrome.test.assertTrue(!!chrome.idle.IdleState.ACTIVE);
    chrome.test.assertTrue(!!chrome.idle.queryState);
    chrome.idle.queryState(1000, function(state) {
      // Depending on the machine, this could come back as either idle or
      // active. However, all we're curious about is the bindings themselves
      // (not the API implementation), so as long as it's a possible response,
      // it's a success for our purposes.
      chrome.test.assertTrue(state == chrome.idle.IdleState.IDLE ||
                             state == chrome.idle.IdleState.ACTIVE);
      chrome.test.succeed();
    });
  },
  function nonexistentApi() {
    chrome.test.assertFalse(!!chrome.nonexistent);
    chrome.test.succeed();
  },
  function disallowedApi() {
    chrome.test.assertFalse(!!chrome.power);
    chrome.test.succeed();
  },
]);
