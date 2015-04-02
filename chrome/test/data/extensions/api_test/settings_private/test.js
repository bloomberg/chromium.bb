// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This just tests the interface. It does not test for specific results, only
// that callbacks are correctly invoked, expected parameters are correct,
// and failures are detected.

var kTestPrefName = 'test.foo_bar';
var kTestPageId = 'pageId';

function callbackResult(result) {
  if (chrome.runtime.lastError)
    chrome.test.fail(chrome.runtime.lastError.message);
  else if (result == false)
    chrome.test.fail('Failed: ' + result);
}

var availableTests = [
  function setPref() {
    chrome.settingsPrivate.setPref(
        kTestPrefName,
        true,
        kTestPageId,
        function(success) {
          callbackResult(success);
          chrome.test.succeed();
        });
  },
  function getPref() {
    chrome.settingsPrivate.getPref(
        kTestPrefName,
        function(value) {
          chrome.test.assertTrue(value !== null);
          callbackResult(true);
          chrome.test.succeed();
        });
  },
  function getAllPrefs() {
    chrome.settingsPrivate.getAllPrefs(
        function(prefs) {
          chrome.test.assertTrue(prefs.length > 0);
          callbackResult(true);
          chrome.test.succeed();
        });
  },
];

var testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));

