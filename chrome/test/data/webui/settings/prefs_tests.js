// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for settings-prefs. */
cr.define('settings_prefs', function() {
  /**
   * Creates a deep copy of the object.
   * @param {!Object} obj
   * @return {!Object}
   */
  function deepCopy(obj) {
    return JSON.parse(JSON.stringify(obj));
  }

  function registerTests() {
    suite('CrSettingsPrefs', function() {
      /**
       * Prefs instance created before each test.
       * @type {SettingsPrefsElement|undefined}
       */
      var prefs;

      /** @type {settings.FakeSettingsPrivate} */
      var fakeApi = null;

      /**
       * @param {!Object} prefStore Pref store from <settings-prefs>.
       * @param {string} key Pref key of the pref to return.
       * @return {chrome.settingsPrivate.PrefObject|undefined}
       */
      function getPrefFromKey(prefStore, key) {
        var path = key.split('.');
        var pref = prefStore;
        for (var part of path) {
          pref = pref[part];
          if (!pref)
            return undefined;
        }
        return pref;
      }

      /**
       * Checks that the fake API pref store contains the expected values.
       * @param {number} testCaseValueIndex The index of possible next values
       *     from the test case to check.
       */
      function assertFakeApiPrefsSet(testCaseValueIndex) {
        for (var testCase of prefsTestCases) {
          var expectedValue = JSON.stringify(
              testCase.nextValues[testCaseValueIndex]);
          var actualValue = JSON.stringify(
              fakeApi.prefs[testCase.pref.key].value);
          assertEquals(expectedValue, actualValue, testCase.pref.key);
        }
      }

      /**
       * Checks that the <settings-prefs> contains the expected values.
       * @param {number} testCaseValueIndex The index of possible next values
       *     from the test case to check.
       */
      function assertPrefsSet(testCaseValueIndex) {
        for (var testCase of prefsTestCases) {
          var expectedValue = JSON.stringify(
              testCase.nextValues[testCaseValueIndex]);
          var actualValue = JSON.stringify(
              prefs.get('prefs.' + testCase.pref.key + '.value'));
          assertEquals(expectedValue, actualValue);
        }
      }

      // Initialize a <settings-prefs> before each test.
      setup(function() {
        PolymerTest.clearBody();

        // Override chrome.settingsPrivate with FakeSettingsPrivate.
        fakeApi = new settings.FakeSettingsPrivate(
            prefsTestCases.map(function(testCase) {
              return testCase.pref;
            }));
        CrSettingsPrefs.deferInitialization = true;

        prefs = document.createElement('settings-prefs');
        prefs.initialize(fakeApi);

        // getAllPrefs is asynchronous, so return the prefs promise.
        return CrSettingsPrefs.initialized;
      });

      teardown(function() {
        CrSettingsPrefs.resetForTesting();
        CrSettingsPrefs.deferInitialization = false;
        prefs.resetForTesting();

        PolymerTest.clearBody();
      });

      test('receives and caches prefs', function testGetPrefs() {
        // Test that each pref has been successfully copied to the Polymer
        // |prefs| property.
        for (var key in fakeApi.prefs) {
          var expectedPref = fakeApi.prefs[key];
          var actualPref = getPrefFromKey(prefs.prefs, key);
          if (!expectNotEquals(undefined, actualPref)) {
            // We've already registered an error, so skip the pref.
            continue;
          }

          assertEquals(JSON.stringify(expectedPref),
                       JSON.stringify(actualPref));
        }
      });

      test('forwards pref changes to API', function testSetPrefs() {
        // Test that settings-prefs uses the setPref API.
        for (var testCase of prefsTestCases) {
          prefs.set('prefs.' + testCase.pref.key + '.value',
                    deepCopy(testCase.nextValues[0]));
        }
        // Check that setPref has been called for the right values.
        assertFakeApiPrefsSet(0);

        // Test that when setPref fails, the pref is reverted locally.
        for (var testCase of prefsTestCases) {
          fakeApi.failNextSetPref();
          prefs.set('prefs.' + testCase.pref.key + '.value',
                    deepCopy(testCase.nextValues[1]));
        }

        assertPrefsSet(0);

        // Test that setPref is not called when the pref doesn't change.
        fakeApi.disallowSetPref();
        for (var testCase of prefsTestCases) {
          prefs.set('prefs.' + testCase.pref.key + '.value',
                    deepCopy(testCase.nextValues[0]));
        }
        assertFakeApiPrefsSet(0);
        fakeApi.allowSetPref();
      });

      test('responds to API changes', function testOnChange() {
        // Changes from the API should not result in those changes being sent
        // back to the API, as this could trigger a race condition.
        fakeApi.disallowSetPref();
        var prefChanges = [];
        for (var testCase of prefsTestCases) {
          prefChanges.push({key: testCase.pref.key,
                            value: testCase.nextValues[0]});
        }

        // Send a set of changes.
        fakeApi.sendPrefChanges(prefChanges);
        assertPrefsSet(0);

        prefChanges = [];
        for (var testCase of prefsTestCases) {
          prefChanges.push({key: testCase.pref.key,
                            value: testCase.nextValues[1]});
        }

        // Send a second set of changes.
        fakeApi.sendPrefChanges(prefChanges);
        assertPrefsSet(1);

        // Send the same set of changes again -- nothing should happen.
        fakeApi.sendPrefChanges(prefChanges);
        assertPrefsSet(1);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
