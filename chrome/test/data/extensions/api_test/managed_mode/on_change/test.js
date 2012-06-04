// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Managed Mode API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.ManagedModeOnChange

// Listen until |event| has fired with all of the values in |expected| in that
// order.
function listenForSequence(event, expected) {
  var done = chrome.test.listenForever(event, function(value) {
    chrome.test.assertEq(expected.shift(), value);
    if (expected.length == 0)
      done();
  });
}

var managedMode = chrome.managedModePrivate;

chrome.test.runTests([
  function testOnChange() {
    var match = document.location.search.match(/^\?expect=(.*)$/);
    chrome.test.assertTrue(match !== null);
    var expected = match[1].split(/,/).map(function(value) {
      return {'value': (value === 'true')};
    });

    listenForSequence(managedMode.onChange, expected);
  }
]);
