// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Error codes and messages.
var kFeatureDisabledCode = "feature_disabled";
var kFeatureDisabledError =
    "[feature_disabled]: Launching ephemeral apps is not enabled";
var kFeatureUserGestureCode = "user_gesture_required";
var kFeatureUserGestureError =
    "[user_gesture_required]: User gesture is required";
var kInstallInProgressCode = "install_in_progress";
var kInstallInProgressError =
    "[install_in_progress]: An install is already in progress";
var kUnsupportedExtensionTypeCode = "unsupported_extension_type";
var kUnsupportedExtensionTypeError =
    "[unsupported_extension_type]: Not an app";

// App ids.
var kDefaultAppId = "kbiancnbopdghkfedjhfdoegjadfjeal";
var kDefaultAppManifestPath = "app/manifest.json";
var kAppWithPermissionsId = "mbfcnecjknjpipkfkoangpfnhhlpamki";
var kAppWithPermissionsManifestPath = "app_with_permissions/manifest.json";
var kExtensionId = "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeid";

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;
var callbackFail = chrome.test.callbackFail;
var callbackPass = chrome.test.callbackPass;

// Returns the string contents of the app's manifest file.
function getManifest(path) {
  // Do a synchronous XHR to get the manifest.
  var xhr = new XMLHttpRequest();
  xhr.open("GET", path, false);
  xhr.send(null);
  return xhr.responseText;
}
