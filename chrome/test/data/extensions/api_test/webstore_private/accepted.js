// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests where the beginInstallWithManifest2 dialog would be auto-accepted
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
    chrome.webstorePrivate.beginInstallWithManifest2(
        { 'id':id, 'manifest':getManifest() }, callbackFail(expectedError));
  },

  function missingVersion() {
    var manifestObj = JSON.parse(getManifest());
    delete manifestObj["version"];
    var manifest = JSON.stringify(manifestObj);
    chrome.webstorePrivate.beginInstallWithManifest2(
        { 'id':extensionId, 'manifest': manifest },
        callbackFail("Invalid manifest", function(result) {
      assertEq("manifest_error", result);
    }));
  },

  function successfulInstall() {
    // See things through all the way to a successful install.
    listenOnce(chrome.management.onInstalled, callbackPass(function(info) {
      assertEq(info.id, extensionId);
    }));

    var manifest = getManifest();
    getIconData(function(icon) {

      // Begin installing.
      chrome.webstorePrivate.beginInstallWithManifest2(
          {'id': extensionId,'iconData': icon, 'manifest': manifest },
          function(result) {
        assertNoLastError();
        assertEq(result, "");

        // Now complete the installation.
        chrome.webstorePrivate.completeInstall(extensionId, callbackPass());
      });
    });
  }
];

runTests(tests);
