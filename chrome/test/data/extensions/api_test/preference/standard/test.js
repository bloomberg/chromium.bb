// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Preferences API test
// Run with browser_tests --gtest_filter=ExtensionPreferenceApiTest.Standard

var pn = chrome.privacy.network;
var ps = chrome.privacy.services;
// The collection of preferences to test, split into objects with a "root"
// (the root object they preferences are exposed on) and a dictionary of
// preference name -> default value.
var preferences_to_test = [
  {
    root: chrome.privacy.network,
    preferences: {
      networkPredictionEnabled: false,
      webRTCMultipleRoutesEnabled: false,
      webRTCNonProxiedUdpEnabled: false,
    }
  },
  {
    root: chrome.privacy.websites,
    preferences: {
      thirdPartyCookiesAllowed: false,
      hyperlinkAuditingEnabled: false,
      referrersEnabled: false,
      doNotTrackEnabled: false,
      protectedContentEnabled: true,
    }
  },
  {
    root: chrome.privacy.services,
    preferences: {
      alternateErrorPagesEnabled: false,
      autofillEnabled: false,
      autofillAddressEnabled: false,
      autofillCreditCardEnabled: false,
      passwordSavingEnabled: false,
      safeBrowsingEnabled: false,
      safeBrowsingExtendedReportingEnabled: false,
      searchSuggestEnabled: false,
      spellingServiceEnabled: false,
      translationServiceEnabled: false,
    }
  },
];

// Some preferences are only present on certain platforms or are hidden
// behind flags and might not be present when this test runs.
var possibly_missing_preferences = new Set([
  'webRTCMultipleRoutesEnabled',         // requires ENABLE_WEBRTC=1
  'webRTCNonProxiedUdpEnabled',          // requires ENABLE_WEBRTC=1
]);

if (!navigator.userAgent.includes('Windows') &&
    !navigator.userAgent.includes('CrOS')) {
  possibly_missing_preferences.add('protectedContentEnabled');
}

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

// Verifies that the preference has the expected default value.
function expectDefault(prefName, defaultValue) {
  return expect({
    value: defaultValue,
    levelOfControl: 'controllable_by_this_extension'
  }, '`' + prefName + '` is expected to be the default, which is ' +
     defaultValue);
}

// Verifies that the preference is properly controlled by the extension.
function expectControlled(prefName, newValue) {
  return expect({
    value: newValue,
    levelOfControl: 'controlled_by_this_extension',
  }, '`' + prefName + '` is expected to be controlled by this extension.');
}

// Tests getting the preference value (which should be uncontrolled and at its
// default value).
function prefGetter(prefName, defaultValue) {
  if (possibly_missing_preferences.has(prefName) && !this[prefName]) {
    return;
  }
  this[prefName].get({}, expectDefault(prefName, defaultValue));
}

// Tests setting the preference value (to the inverse of the default, so that
// it should be controlled by this extension).
function prefSetter(prefName, defaultValue) {
  if (possibly_missing_preferences.has(prefName) && !this[prefName]) {
    return;
  }
  this[prefName].set({value: !defaultValue},
                     chrome.test.callbackPass(function() {
    this[prefName].get({}, expectControlled(prefName, !defaultValue));
  }.bind(this)));
}

chrome.test.runTests([
  function getPreferences() {
    for (let preferenceSet of preferences_to_test) {
      for (let key in preferenceSet.preferences) {
        prefGetter.call(preferenceSet.root, key,
                        preferenceSet.preferences[key]);
      }
    }
  },
  function setGlobals() {
    for (let preferenceSet of preferences_to_test) {
      for (let key in preferenceSet.preferences) {
        prefSetter.call(preferenceSet.root, key,
                        preferenceSet.preferences[key]);
      }
    }
  },
  // Set the WebRTCIPHhandlingPolicy and verify it in the get function.
  function testWebRTCIPHandlingPolicy() {
    if (pn.webRTCIPHandlingPolicy == undefined) {
      chrome.test.callbackPass();
      return;
    }
    pn.webRTCIPHandlingPolicy.get(
      {},
      expect(
        {value: chrome.privacy.IPHandlingPolicy.DEFAULT_PUBLIC_INTERFACE_ONLY,
         levelOfControl: 'controllable_by_this_extension'},
        'should receive default_public_interface_only.'));

    pn.webRTCIPHandlingPolicy.set(
      {value: chrome.privacy.IPHandlingPolicy.DISABLE_NON_PROXIED_UDP});

    pn.webRTCIPHandlingPolicy.get(
      {},
      expect(
        {value: chrome.privacy.IPHandlingPolicy.DISABLE_NON_PROXIED_UDP,
         levelOfControl: 'controlled_by_this_extension'},
        'should receive disable_non_proxied_udp.'));
  },
  // Setting autofillEnabled should also set autofillAddressEnabled and
  // autofillCreditCardEnabled.
  function testSetAutofillEnabled() {
    ps.autofillEnabled.set(
      { value: false },
      function() {
        ps.autofillAddressEnabled.get(
          {},
          expect(
            {value: false,
             levelOfControl: 'controlled_by_this_extension'},
            'autofillAddressEnabled should be disabled.'));

        ps.autofillCreditCardEnabled.get(
          {},
          expect(
            {value: false,
             levelOfControl: 'controlled_by_this_extension'},
            'autofillCreditCardEnabled should be disabled.'));

        ps.autofillEnabled.set(
          { value: true },
          function() {
            ps.autofillAddressEnabled.get(
              {},
              expect(
                {value: true,
                 levelOfControl: 'controlled_by_this_extension'},
                'autofillAddressEnabled should be enabled.'));

            ps.autofillCreditCardEnabled.get(
              {},
              expect(
                {value: true,
                 levelOfControl: 'controlled_by_this_extension'},
                'autofillCreditCardEnabled should be enabled.'));
          });
      });
  }
]);
