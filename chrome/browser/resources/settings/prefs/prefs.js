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

  Polymer({
    is: 'cr-settings-prefs',

    properties: {
      /**
       * Object containing all preferences.
       */
      settings: {
        type: Object,
        value: function() { return {}; },
        notify: true,
      },
    },

    /** @override */
    created: function() {
      CrSettingsPrefs.isInitialized = false;

      chrome.settingsPrivate.onPrefsChanged.addListener(
          this.onPrefsChanged_.bind(this));
      chrome.settingsPrivate.getAllPrefs(this.onPrefsFetched_.bind(this));
    },

    /**
     * Called when prefs in the underlying Chrome pref store are changed.
     * @param {!Array<!chrome.settingsPrivate.PrefObject>} prefs The prefs that
     *     changed.
     * @private
     */
    onPrefsChanged_: function(prefs) {
      this.updatePrefs_(prefs, false);
    },

    /**
     * Called when prefs are fetched from settingsPrivate.
     * @param {!Array<!chrome.settingsPrivate.PrefObject>} prefs
     * @private
     */
    onPrefsFetched_: function(prefs) {
      this.updatePrefs_(prefs, true);

      CrSettingsPrefs.isInitialized = true;
      document.dispatchEvent(new Event(CrSettingsPrefs.INITIALIZED));
    },


    /**
     * Updates the settings model with the given prefs.
     * @param {!Array<!chrome.settingsPrivate.PrefObject>} prefs
     * @param {boolean} shouldObserve Whether to add an ObjectObserver for each
     *     of the prefs.
     * @private
     */
    updatePrefs_: function(prefs, shouldObserve) {
      prefs.forEach(function(prefObj) {
        let root = this.settings;
        let tokens = prefObj.key.split('.');

        assert(tokens.length > 0);

        for (let i = 0; i < tokens.length; i++) {
          let token = tokens[i];

          if (!root.hasOwnProperty(token)) {
            root[token] = {};
          }
          root = root[token];
        }

        // NOTE: Do this copy rather than just a re-assignment, so that the
        // ObjectObserver fires.
        for (let objKey in prefObj) {
          root[objKey] = prefObj[objKey];
        }

        if (shouldObserve) {
          let keyObserver = new ObjectObserver(root);
          keyObserver.open(
              this.propertyChangeCallback_.bind(this, prefObj.key));
        }
      }, this);
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

        chrome.settingsPrivate.setPref(
            propertyPath,
            newValue,
            /* pageId */ '',
            /* callback */ function() {});
      }
    },
  });
})();
