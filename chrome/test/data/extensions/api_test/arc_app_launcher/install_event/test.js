// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var EXPECTED_APP_ID = null;

chrome.test.runTests([
  function getConfig() {
    chrome.test.getConfig(chrome.test.callbackPass(config => {
      EXPECTED_APP_ID = config.customArg;
    }));
  },

  function onInstalled() {
    chrome.test.assertTrue(!!EXPECTED_APP_ID);
    chrome.test.listenOnce(chrome.arcAppsPrivate.onInstalled, appInfo => {
      chrome.test.assertEq({id: EXPECTED_APP_ID}, appInfo);
      chrome.arcAppsPrivate.launchApp(appInfo.id, chrome.test.callbackPass());
    });
    chrome.test.sendMessage('ready');
  }
]);