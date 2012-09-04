// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // Tests that attaching and detaching to an event for which we don't have
  // permission acts as expected (e.g. we don't DCHECK!).
  function attachAndDetachNoPermisssions() {
    function dummy() {};
    try {
      chrome.management.onEnabled.addListener(dummy);
      chrome.test.fail();
    } catch (e) {
      chrome.test.assertTrue(
          e.message.search("You do not have permission") >= 0,
          e.message);
    }
    chrome.test.assertFalse(chrome.management.onEnabled.hasListeners());
    // Browser should not DCHECK.
    chrome.management.onEnabled.removeListener(dummy);
    chrome.test.succeed();
  },

  // Tests that attaching a named event twice will fail.
  function doubleAttach() {
    function dummy() {};
    var onClicked = new chrome.Event("browserAction.onClicked");
    var onClicked2 = new chrome.Event("browserAction.onClicked");
    onClicked.addListener(dummy);
    chrome.test.assertTrue(onClicked.hasListeners());
    try {
      onClicked2.addListener(dummy);
      chrome.test.fail();
    } catch (e) {
      chrome.test.assertTrue(
          e.message.search("already attached") >= 0,
          e.message);
    }
    chrome.test.assertFalse(onClicked2.hasListeners());
    onClicked2.removeListener(dummy);

    onClicked.removeListener(dummy);
    chrome.test.assertFalse(onClicked.hasListeners());
    chrome.test.succeed();
  },

  // Tests that 2 pages attaching to the same event does not trigger a DCHECK.
  function twoPageAttach() {
    // Test harness should already have opened tab.html, which registers this
    // listener.
    chrome.browserAction.onClicked.addListener(function() {});

    // Test continues in twoPageAttach.html.
    window.open("twoPageAttach.html");
  },
]);
