// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  var EXPECTED_APP_ID = null;

  chrome.test.runTests([
    function getConfig() {
      chrome.test.getConfig(chrome.test.callbackPass(config => {
        EXPECTED_APP_ID = config.customArg;
      }));
    },

    function getAppIdAndLaunchApp() {
      chrome.arcAppsPrivate.launchApp(
          'invalid app id', chrome.test.callbackFail('Launch failed'));
      chrome.test.assertTrue(!!EXPECTED_APP_ID);
      chrome.arcAppsPrivate.getLaunchableApps(
          chrome.test.callbackPass(appsInfo => {
            chrome.test.assertEq([{id: EXPECTED_APP_ID}], appsInfo);
            chrome.arcAppsPrivate.launchApp(
                appsInfo[0].id, chrome.test.callbackPass());
          }));
    }
  ]);
});
