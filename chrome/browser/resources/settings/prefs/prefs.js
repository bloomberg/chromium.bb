/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * @fileoverview
 * 'cr-settings-prefs' is an element which serves as a model for
 * interaction with settings which are stored in Chrome's
 * Preferences.
 *
 * Example:
 *
 *    <cr-settings-prefs id="prefs"></cr-settings-prefs>
 *    <cr-settings-a11y-page prefs="{{this.$.prefs}}"></cr-settings-a11y-page>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-a11y-page
 */
(function() {
  'use strict';

  /**
   * A list of all pref paths used on all platforms in the UI.
   * TODO(jlklein): This is a temporary workaround that needs to be removed
   * once settingsPrivate is implemented with the fetchAll function. We will
   * not need to tell the settingsPrivate API which prefs to fetch.
   * @const {!Array<string>}
   */
  var PREFS_TO_FETCH = [
    'download.default_directory',
    'download.prompt_for_download',
  ];

  /**
   * A list of all CrOS-only pref paths used in the UI.
   * TODO(jlklein): This is a temporary workaround that needs to be removed
   * once settingsPrivate is implemented with the fetchAll function. We will
   * not need to tell the settingsPrivate API which prefs to fetch.
   * @const {!Array<string>}
   */
  var CROS_ONLY_PREFS = [
    'settings.accessibility',
    'settings.a11y.autoclick',
    'settings.a11y.autoclick_delay_ms',
    'settings.a11y.enable_menu',
    'settings.a11y.high_contrast_enabled',
    'settings.a11y.large_cursor_enabled',
    'settings.a11y.screen_magnifier',
    'settings.a11y.sticky_keys_enabled',
    'settings.a11y.virtual_keyboard',
    'settings.touchpad.enable_tap_dragging',
  ];

  Polymer('cr-settings-prefs', {
    publish: {
      /**
       * Object containing all preferences.
       *
       * @attribute settings
       * @type CrSettingsPrefs.Settings
       * @default null
       */
      settings: null,
    },

    /** @override */
    created: function() {
      this.settings = {};
      this.fetchSettings_();
    },

    /**
     * Fetches all settings from settingsPrivate.
     * TODO(jlklein): Implement using settingsPrivate when it's available.
     * @private
     */
    fetchSettings_: function() {
      // *Sigh* We need to export the function name to global scope. This is
      // needed the CoreOptionsHandler can only call a function in the global
      // scope.
      var callbackName = 'CrSettingsPrefs_onPrefsFetched';
      window[callbackName] = this.onPrefsFetched_.bind(this);
      var prefsToFetch = PREFS_TO_FETCH;
      if (cr.isChromeOS)
        prefsToFetch.concat(CROS_ONLY_PREFS);

      chrome.send('fetchPrefs', [callbackName].concat(prefsToFetch));
    },

    /**
     * Fetches all settings from settingsPrivate.
     * @param {!Object} dict Map of fetched property values.
     * @private
     */
    onPrefsFetched_: function(dict) {
      this.parsePrefDict_('', dict);
    },

    /**
     * Helper function for parsing the prefs dict and constructing Preference
     * objects.
     * @param {string} prefix The namespace prefix of the pref.
     * @param {!Object} dict Map with preference values.
     * @private
     */
    parsePrefDict_: function(prefix, dict) {
      for (let prefName in dict) {
        let prefObj = dict[prefName];
        if (!this.isPrefObject_(prefObj)) {
          this.parsePrefDict_(prefix + prefName + '.', prefObj);
          continue;
        }

        // prefObj is actually a pref object. Construct the path to pref using
        // prefix, add the pref to this.settings, and observe changes.
        let root = this.settings;
        let pathPieces = prefix.slice(0, -1).split('.');
        pathPieces.forEach(function(pathPiece) {
          root[pathPiece] = root[pathPiece] || {};
          root = root[pathPiece];
        });

        root[prefName] = prefObj;
        let keyObserver = new ObjectObserver(prefObj);
        keyObserver.open(
            this.propertyChangeCallback_.bind(this, prefix + prefName));
      }
    },

    /**
     * Determines whether the passed object is a pref object from Chrome.
     * @param {*} rawPref The object to check.
     * @return {boolean} True if the passes object is a pref.
     * @private
     */
    isPrefObject_: function(rawPref) {
      return rawPref && typeof rawPref == 'object' &&
          rawPref.hasOwnProperty('value') &&
          rawPref.hasOwnProperty('disabled');
    },

    /**
     * Called when a property of a pref changes.
     * @param {string} propertyPath The path before the property names.
     * @param {!Array<string>} added An array of keys which were added.
     * @param {!Array<string>} removed An array of keys which were removed.
     * @param {!Array<string>} changed An array of keys of properties whose
     *     values changed.
     * @param {function(string) : *} getOldValueFn A function which takes a
     *     property name and returns the old value for that property.
     * @private
     */
    propertyChangeCallback_: function(
        propertyPath, added, removed, changed, getOldValueFn) {
      for (let property in changed) {
        // UI should only be able to change the value of a setting for now, not
        // disabled, etc.
        assert(property == 'value');

        let newValue = changed[property];
        assert(newValue !== undefined);

        switch (typeof newValue) {
          case 'boolean':
            this.setBooleanPref_(
                propertyPath, /** @type {boolean} */ (newValue));
            break;
          case 'number':
            this.setNumberPref_(
                propertyPath, /** @type {number} */ (newValue));
            break;
          case 'string':
            this.setStringPref_(
                propertyPath, /** @type {string} */ (newValue));
            break;
          case 'object':
            assertInstanceof(newValue, Array);
            this.setArrayPref_(
                propertyPath, /** @type {!Array} */ (newValue));
        }
      }
    },

    /**
     * @param {string} propertyPath The full path of the pref.
     * @param {boolean} value The new value of the pref.
     * @private
     */
    setBooleanPref_: function(propertyPath, value) {
      chrome.send('setBooleanPref', [propertyPath, value]);
    },

    /**
     * @param {string} propertyPath The full path of the pref.
     * @param {string} value The new value of the pref.
     * @private
     */
    setStringPref_: function(propertyPath, value) {
      chrome.send('setStringPref', [propertyPath, value]);
    },

    /**
     * @param {string} propertyPath The full path of the pref.
     * @param {number} value The new value of the pref.
     * @private
     */
    setNumberPref_: function(propertyPath, value) {
      var setFn = value % 1 == 0 ? 'setIntegerPref' : 'setDoublePref';
      chrome.send(setFn, [propertyPath, value]);
    },

    /**
     * @param {string} propertyPath The full path of the pref.
     * @param {!Array} value The new value of the pref.
     * @private
     */
    setArrayPref_: function(propertyPath, value) {
      chrome.send('setListPref', [propertyPath, value]);
    },
  });
})();
