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

  /**
   * Mock of chrome.settingsPrivate API.
   * @constructor
   * @extends {chrome.settingsPrivate}
   */
  function MockSettingsApi() {
    this.prefs = {};

    // Hack alert: bind this instance's onPrefsChanged members to this.
    this.onPrefsChanged = {
      addListener: this.onPrefsChanged.addListener.bind(this),
      removeListener: this.onPrefsChanged.removeListener.bind(this),
    };

    for (var testCase of prefsTestCases)
      this.addPref_(testCase.type, testCase.key, testCase.values[0]);
  }

  // Make the listener static because it refers to a singleton.
  MockSettingsApi.listener_ = null;

  MockSettingsApi.prototype = {
    // chrome.settingsPrivate overrides.
    onPrefsChanged: {
      addListener: function(listener) {
        MockSettingsApi.listener_ = listener;
      },

      removeListener: function(listener) {
        MockSettingsApi.listener_ = null;
      },
    },

    getAllPrefs: function(callback) {
      // Send a copy of prefs to keep our internal state private.
      var prefs = [];
      for (var key in this.prefs)
        prefs.push(deepCopy(this.prefs[key]));

      // Run the callback asynchronously to test that the prefs aren't actually
      // used before they become available.
      setTimeout(callback.bind(null, prefs));
    },

    setPref: function(key, value, pageId, callback) {
      var pref = this.prefs[key];
      assertNotEquals(undefined, pref);
      assertEquals(typeof value, typeof pref.value);
      assertEquals(Array.isArray(value), Array.isArray(pref.value));

      if (this.failNextSetPref_) {
        callback(false);
        this.failNextSetPref_ = false;
        return;
      }
      assertNotEquals(true, this.disallowSetPref_);

      var changed = JSON.stringify(pref.value) != JSON.stringify(value);
      pref.value = deepCopy(value);
      callback(true);

      // Like chrome.settingsPrivate, send a notification when prefs change.
      if (changed)
        this.sendPrefChanges([{key: key, value: deepCopy(value)}]);
    },

    getPref: function(key, callback) {
      var pref = this.prefs[key];
      assertNotEquals(undefined, pref);
      callback(deepCopy(pref));
    },

    // Functions used by tests.

    /** Instructs the API to return a failure when setPref is next called. */
    failNextSetPref: function() {
      this.failNextSetPref_ = true;
    },

    /** Instructs the API to assert (fail the test) if setPref is called. */
    disallowSetPref: function() {
      this.disallowSetPref_ = true;
    },

    allowSetPref: function() {
      this.disallowSetPref_ = false;
    },

    /**
     * Notifies the listener of pref changes.
     * @param {!Object<{key: string, value: *}>} changes
     */
    sendPrefChanges: function(changes) {
      var prefs = [];
      for (var change of changes) {
        var pref = this.prefs[change.key];
        assertNotEquals(undefined, pref);
        pref.value = change.value;
        prefs.push(deepCopy(pref));
      }
      MockSettingsApi.listener_(prefs);
    },

    // Private methods for use by the mock API.

    /**
     * @param {!chrome.settingsPrivate.PrefType} type
     * @param {string} key
     * @param {*} value
     */
    addPref_: function(type, key, value) {
      this.prefs[key] = {
        type: type,
        key: key,
        value: value,
      };
    },
  };

  function registerTests() {
    suite('CrSettingsPrefs', function() {
      /**
       * Prefs instance created before each test.
       * @type {CrSettingsPrefsElement|undefined}
       */
      var prefs;

      /** @type {MockSettingsApi} */
      var mockApi = null;

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
       * Checks that the mock API pref store contains the expected values.
       * @param {number} testCaseValueIndex The index of possible values from
       *     the test case to check.
       */
      function assertMockApiPrefsSet(testCaseValueIndex) {
        for (var testCase of prefsTestCases) {
          var expectedValue = JSON.stringify(
              testCase.values[testCaseValueIndex]);
          var actualValue = JSON.stringify(mockApi.prefs[testCase.key].value);
          assertEquals(expectedValue, actualValue);
        }
      }

      /**
       * Checks that the <settings-prefs> contains the expected values.
       * @param {number} testCaseValueIndex The index of possible values from
       *     the test case to check.
       */
      function assertPrefsSet(testCaseValueIndex) {
        for (var testCase of prefsTestCases) {
          var expectedValue = JSON.stringify(
              testCase.values[testCaseValueIndex]);
          var actualValue = JSON.stringify(
              prefs.get('prefs.' + testCase.key + '.value'));
          assertEquals(expectedValue, actualValue);
        }
      }

      /**
       * List of CrSettingsPref elements created for testing.
       * @type {!Array<!CrSettingsPrefs>}
       */
      var createdElements = [];

      // Initialize <settings-prefs> elements before each test.
      setup(function() {
        mockApi = new MockSettingsApi();
        // TODO(michaelpg): don't use global variables to inject the API.
        window.mockApi = mockApi;

        // Create and attach the <settings-prefs> elements. Make several of
        // them to test that the shared state model scales correctly.
        createdElements = [];
        for (var i = 0; i < 100; i++) {
          var prefsInstance = document.createElement('settings-prefs');
          document.body.appendChild(prefsInstance);
          createdElements.push(prefsInstance);
        }
        // For simplicity, only use one prefs element in the tests. Use an
        // arbitrary index instead of the first or last element created.
        prefs = createdElements[42];

        // getAllPrefs is asynchronous, so return the prefs promise.
        return CrSettingsPrefs.initialized;
      });

      teardown(function() {
        CrSettingsPrefs.resetForTesting();

        // Reset each <settings-prefs>.
        for (var i = 0; i < createdElements.length; i++)
          createdElements[i].resetForTesting();

        PolymerTest.clearBody();
        window.mockApi = undefined;
      });

      test('receives and caches prefs', function() {
        // Test that each pref has been successfully copied to the Polymer
        // |prefs| property.
        for (var key in mockApi.prefs) {
          var expectedPref = mockApi.prefs[key];
          var actualPref = getPrefFromKey(prefs.prefs, key);
          if (!expectNotEquals(undefined, actualPref)) {
            // We've already registered an error, so skip the pref.
            continue;
          }

          assertEquals(JSON.stringify(expectedPref),
                       JSON.stringify(actualPref));
        }
      });

      test('forwards pref changes to API', function() {
        // Test that settings-prefs uses the setPref API.
        for (var testCase of prefsTestCases) {
          prefs.set('prefs.' + testCase.key + '.value',
                    deepCopy(testCase.values[1]));
        }
        // Check that setPref has been called for the right values.
        assertMockApiPrefsSet(1);

        // Test that when setPref fails, the pref is reverted locally.
        for (var testCase of prefsTestCases) {
          mockApi.failNextSetPref();
          prefs.set('prefs.' + testCase.key + '.value',
                    deepCopy(testCase.values[2]));
        }

        assertPrefsSet(1);

        // Test that setPref is not called when the pref doesn't change.
        mockApi.disallowSetPref();
        for (var testCase of prefsTestCases) {
          prefs.set('prefs.' + testCase.key + '.value',
                    deepCopy(testCase.values[1]));
        }
        assertMockApiPrefsSet(1);
        mockApi.allowSetPref();
      });

      test('responds to API changes', function() {
        // Changes from the API should not result in those changes being sent
        // back to the API, as this could trigger a race condition.
        mockApi.disallowSetPref();
        var prefChanges = [];
        for (var testCase of prefsTestCases)
          prefChanges.push({key: testCase.key, value: testCase.values[1]});

        // Send a set of changes.
        mockApi.sendPrefChanges(prefChanges);
        assertPrefsSet(1);

        prefChanges = [];
        for (var testCase of prefsTestCases)
          prefChanges.push({key: testCase.key, value: testCase.values[2]});

        // Send a second set of changes.
        mockApi.sendPrefChanges(prefChanges);
        assertPrefsSet(2);

        // Send the same set of changes again -- nothing should happen.
        mockApi.sendPrefChanges(prefChanges);
        assertPrefsSet(2);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
