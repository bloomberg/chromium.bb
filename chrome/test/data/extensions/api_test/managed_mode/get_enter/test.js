// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Managed Mode API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.ManagedModeApi

var managedMode = chrome.managedModePrivate;

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

chrome.test.runTests([
  function disabledByDefault() {
    managedMode.get(expect({value: false},
                    'Managed mode should be disabled.'));
  },

  // This will need to be modified once entering managed mode requires user
  // confirmation.
  function enteringSucceeds() {
    managedMode.enter(expect({success: true},
                      'Should be able to enter managed mode.'));

    managedMode.get(expect({value: true},
                    'Managed mode should be on once it has been entered.'));
  },

  function enterTwice() {
    managedMode.enter(expect({success: true},
                      'Should be able to enter managed mode.'));
    managedMode.enter(expect({success: true},
                      'Should be able to enter managed mode twice.'));
  }
]);
