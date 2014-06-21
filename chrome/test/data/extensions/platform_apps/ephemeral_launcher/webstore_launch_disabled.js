// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [

  // Verifies that getEphemeralAppsEnabled() will return false if the feature
  // is not enabled.
  function canLaunchEphemeralApp() {
    chrome.webstorePrivate.getEphemeralAppsEnabled(
        callbackPass(function(isEnabled) {
          assertFalse(isEnabled);
        }));
  },

  // Verifies that if the feature is not enabled, launchEphemeralApp() will not
  // succeed.
  function launchEphemeralApp() {
    chrome.test.runWithUserGesture(function() {
      chrome.webstorePrivate.launchEphemeralApp(
          kDefaultAppId,
          callbackFail(kFeatureDisabledError, function(result) {
            assertEq(kFeatureDisabledCode, result);
          }));
    });
  }

];

chrome.test.runTests(tests);
