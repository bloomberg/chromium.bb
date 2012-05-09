// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Preferences API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.PreferenceApi

var preferences_to_test = [
  {
    root: chrome.privacy.network,
    preferences: [
      'networkPredictionEnabled'
    ]
  },
  {
    root: chrome.privacy.websites,
    preferences: [
      'thirdPartyCookiesAllowed',
      'hyperlinkAuditingEnabled',
      'referrersEnabled',
      'protectedContentEnabled'
    ]
  },
  {
    root: chrome.privacy.services,
    preferences: [
      'alternateErrorPagesEnabled',
      'autofillEnabled',
      'instantEnabled',
      'safeBrowsingEnabled',
      'searchSuggestEnabled',
      'spellingServiceEnabled',
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
  if (pref === 'protectedContentEnabled' && !this[pref]) {
    // `protectedContentEnabled` is ChromeOS only, so it might not exist when
    // this test runs, and that's pretty much OK.
    return true;
  }
  this[pref].get({}, expectFalse(pref));
}

function prefSetter(pref) {
  if (pref === 'protectedContentEnabled' && !this[pref]) {
    // `protectedContentEnabled` is ChromeOS only, so it might not exist when
    // this test runs, and that's pretty much OK.
    return true;
  }
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
