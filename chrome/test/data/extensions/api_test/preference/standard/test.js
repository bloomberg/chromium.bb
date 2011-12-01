// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Preferences API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.PreferenceApi

var preferences_to_test = [
  {
    root: chrome.experimental.privacy.network,
    preferences: [
      'networkPredictionEnabled'
    ]
  },
  {
    root: chrome.experimental.privacy.websites,
    preferences: [
      'thirdPartyCookiesAllowed',
      'hyperlinkAuditingEnabled',
      'referrersEnabled'
    ]
  },
  {
    root: chrome.experimental.privacy.services,
    preferences: [
      'alternateErrorPagesEnabled',
      'autofillEnabled',
      'instantEnabled',
      // TODO(mkwst): 'metricsReportingEnabled',
      'safeBrowsingEnabled',
      'searchSuggestEnabled',
      'translationServiceEnabled'
    ]
  },
];

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

function expectFalse(pref) {
  return expect({
    value: false,
    levelOfControl: 'controllable_by_this_extension'
  }, '`' + pref + '` is expected to be false.');
}

function prefGetter(pref) {
  this[pref].get({}, expectFalse(pref));
}

function prefSetter(pref) {
  this[pref].set({value: true}, chrome.test.callbackPass());
}

chrome.test.runTests([
  function getPreferences() {
    for (var i = 0; i < preferences_to_test.length; i++) {
      preferences_to_test[i].preferences.forEach(
          prefGetter.bind(preferences_to_test[i].root));
    }
  },
  function setGlobals() {
    for (var i = 0; i < preferences_to_test.length; i++) {
      preferences_to_test[i].preferences.forEach(
          prefSetter.bind(preferences_to_test[i].root));
    }
  }
]);
