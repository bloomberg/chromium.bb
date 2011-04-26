// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests where the beginInstallWithManifest dialog would be auto-accepted
// (including a few cases where this does not matter).

var tests = [

  function completeBeforeBegin() {
    var expectedError =
        extensionId + " does not match a previous call to beginInstall";
    chrome.webstorePrivate.completeInstall(extensionId,
                                           callbackFail(expectedError));
  },

  function invalidID() {
    var expectedError = "Invalid id";
    var id = "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";
    chrome.webstorePrivate.beginInstallWithManifest(
        id, "", getManifest(), callbackFail(expectedError));
  },

  function missingVersion() {
    var manifestObj = JSON.parse(getManifest());
    delete manifestObj["version"];
    var manifest = JSON.stringify(manifestObj);
    chrome.webstorePrivate.beginInstallWithManifest(
        extensionId,
        "",
        manifest,
        callbackFail("Invalid manifest", function(result) {
      assertEq("manifest_error", result);
    }));
  },

  function successfulInstall() {
    // See things through all the way to a successful install, and then we
    // uninstall the extension to clean up.
    listenOnce(chrome.management.onInstalled, function(info) {
      assertEq(info.id, extensionId);
      chrome.management.uninstall(extensionId, callbackPass());
    });

    var manifest = getManifest();
    getIconData(function(icon) {

      // Begin installing.
      chrome.webstorePrivate.beginInstallWithManifest(extensionId,
                                                      icon,
                                                      manifest,
                                                      function(result) {
        assertNoLastError();
        assertEq(result, "");

        // Now complete the installation.
        chrome.webstorePrivate.completeInstall(extensionId, function() {
          assertNoLastError();
        });
      });
    });
  }
];

runTests(tests);
