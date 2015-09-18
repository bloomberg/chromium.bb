// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-settings-prefs. */
cr.define('cr_settings_prefs', function() {
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
    this.listener_ = null;

    // Hack alert: bind this instance's onPrefsChanged members to this.
    this.onPrefsChanged = {
      addListener: this.onPrefsChanged.addListener.bind(this),
      removeListener: this.onPrefsChanged.removeListener.bind(this),
    };

    for (var testCase of prefsTestCases)
      this.addPref_(testCase.type, testCase.key, testCase.values[0]);
  }

  MockSettingsApi.prototype = {
    // chrome.settingsPrivate overrides.
    onPrefsChanged: {
      addListener: function(listener) {
        this.listener_ = listener;
      },

      removeListener: function(listener) {
        expectNotEquals(null, this.listener_);
        this.listener_ = null;
      },
    },

    getAllPrefs: function(callback) {
      // Send a copy of prefs to keep our internal state private.
      var prefs = [];
      for (var key in this.prefs)
        prefs.push(deepCopy(this.prefs[key]));

      callback(prefs);
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
      this.listener_(prefs);
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
       * @type {CrSettingsPrefs}
       */
      var prefs;

      /** @type {MockSettingsApi} */
      var mockApi = null;

      /**
       * @param {!Object} prefStore Pref store from <cr-settings-prefs>.
       * @param {string} key Pref key of the pref to return.
       * @return {(chrome.settingsPrivate.PrefObject|undefined)}
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
      function expectMockApiPrefsSet(testCaseValueIndex) {
        for (var testCase of prefsTestCases) {
          var expectedValue = JSON.stringify(
              testCase.values[testCaseValueIndex]);
          var actualValue = JSON.stringify(mockApi.prefs[testCase.key].value);
          expectEquals(expectedValue, actualValue);
        }
      }

      /**
       * Checks that the <cr-settings-prefs> contains the expected values.
       * @param {number} testCaseValueIndex The index of possible values from
       *     the test case to check.
       */
      function expectPrefsSet(testCaseValueIndex) {
        for (var testCase of prefsTestCases) {
          var expectedValue = JSON.stringify(
              testCase.values[testCaseValueIndex]);
          var actualValue = JSON.stringify(
              prefs.get('prefs.' + testCase.key + '.value'));
          expectEquals(expectedValue, actualValue);
        }
      }

      // Initialize a <cr-settings-prefs> element before each test.
      setup(function(done) {
        mockApi = new MockSettingsApi();
        // TODO(michaelpg): don't use global variables to inject the API.
        window.mockApi = mockApi;

        // Create and attach the <cr-settings-prefs> element.
        PolymerTest.clearBody();
        prefs = document.createElement('cr-settings-prefs');
        document.body.appendChild(prefs);

        window.mockApi = undefined;

        // Wait for CrSettingsPrefs.INITIALIZED.
        if (!CrSettingsPrefs.isInitialized) {
          var listener = function() {
            document.removeEventListener(CrSettingsPrefs.INITIALIZED, listener);
            done();
          };
          document.addEventListener(CrSettingsPrefs.INITIALIZED, listener);
          return;
        }

        done();
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

          expectEquals(JSON.stringify(expectedPref),
                       JSON.stringify(actualPref));
        }
      });

      test('forwards pref changes to API', function() {
        // Test that cr-settings-prefs uses the setPref API.
        for (var testCase of prefsTestCases) {
          prefs.set('prefs.' + testCase.key + '.value',
                    deepCopy(testCase.values[1]));
        }
        // Check that setPref has been called for the right values.
        expectMockApiPrefsSet(1);

        // Test that when setPref fails, the pref is reverted locally.
        for (var testCase of prefsTestCases) {
          mockApi.failNextSetPref();
          prefs.set('prefs.' + testCase.key + '.value',
                    deepCopy(testCase.values[2]));
        }

        expectPrefsSet(1);

        // Test that setPref is not called when the pref doesn't change.
        mockApi.disallowSetPref();
        for (var testCase of prefsTestCases) {
          prefs.set('prefs.' + testCase.key + '.value',
                    deepCopy(testCase.values[1]));
        }
        expectMockApiPrefsSet(1);
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
        expectPrefsSet(1);

        prefChanges = [];
        for (var testCase of prefsTestCases)
          prefChanges.push({key: testCase.key, value: testCase.values[2]});

        // Send a second set of changes.
        mockApi.sendPrefChanges(prefChanges);
        expectPrefsSet(2);

        // Send the same set of changes again -- nothing should happen.
        mockApi.sendPrefChanges(prefChanges);
        expectPrefsSet(2);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
