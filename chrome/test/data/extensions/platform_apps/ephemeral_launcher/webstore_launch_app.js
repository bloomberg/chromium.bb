// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [

  // Verifies that getEphemeralAppsEnabled() will return true.
  function canLaunchEphemeralApp() {
    chrome.webstorePrivate.getEphemeralAppsEnabled(
        callbackPass(function(isEnabled) {
          assertTrue(isEnabled);
        }));
  },

  // Verifies that launchEphemeralApp() will fail without a user gesture.
  function noUserGesture() {
    chrome.webstorePrivate.launchEphemeralApp(
        kDefaultAppId,
        callbackFail(kFeatureUserGestureError, function(result) {
          assertEq(kFeatureUserGestureCode, result);
        }));
  },

  // Test an attempt to launch an extension.
  function launchExtension() {
    chrome.test.runWithUserGesture(function() {
      chrome.webstorePrivate.launchEphemeralApp(
          kExtensionId,
          callbackFail(kUnsupportedExtensionTypeError, function(result) {
            assertEq(kUnsupportedExtensionTypeCode, result);
          }));
    });
  },

  // Test a successful ephemeral install and launch.
  function launchSuccess() {
    chrome.test.runWithUserGesture(function() {
      chrome.webstorePrivate.launchEphemeralApp(
          kDefaultAppId,
          callbackPass(function(result) {
            assertEq("success", result);
          }));
    });
  },

  // Verifies that launchEphemeralApp() will fail if a full install is in
  // progress.
  function pendingInstall() {
    // First initiate a full install of the app.
    var manifest = getManifest(kAppWithPermissionsManifestPath);
    chrome.webstorePrivate.beginInstallWithManifest3(
        { "id": kAppWithPermissionsId, "manifest": manifest },
        callbackPass(function(result) {
          assertEq(result, "");

          // Attempt to launch the app ephemerally.
          chrome.test.runWithUserGesture(function() {
            chrome.webstorePrivate.launchEphemeralApp(
                kAppWithPermissionsId,
                callbackFail(kInstallInProgressError, function(result) {
                  assertEq(kInstallInProgressCode, result);
                }));
        });
    }));
  }

];

chrome.test.runTests(tests);
