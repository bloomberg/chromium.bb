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
      prefStore: {
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
     * @param {!Array<!PrefObject>} prefs The prefs that changed.
     * @private
     */
    onPrefsChanged_: function(prefs) {
      this.updatePrefs_(prefs, false);
    },

    /**
     * Called when prefs are fetched from settingsPrivate.
     * @param {!Array<!PrefObject>} prefs
     * @private
     */
    onPrefsFetched_: function(prefs) {
      this.updatePrefs_(prefs, true);

      CrSettingsPrefs.isInitialized = true;
      document.dispatchEvent(new Event(CrSettingsPrefs.INITIALIZED));
    },


    /**
     * Updates the settings model with the given prefs.
     * @param {!Array<!PrefObject>} prefs
     * @param {boolean} shouldObserve Whether each of the prefs should be
     *     observed.
     * @private
     */
    updatePrefs_: function(prefs, shouldObserve) {
      prefs.forEach(function(prefObj) {
        let root = this.prefStore;
        let tokens = prefObj.key.split('.');

        assert(tokens.length > 0);

        for (let i = 0; i < tokens.length; i++) {
          let token = tokens[i];

          if (!root.hasOwnProperty(token)) {
            let path = 'prefStore.' + tokens.slice(0, i + 1).join('.');
            this.set(path, {});
          }
          root = root[token];
        }

        // NOTE: Do this copy rather than just a re-assignment, so that the
        // observer fires.
        for (let objKey in prefObj) {
          let path = 'prefStore.' + prefObj.key + '.' + objKey;

          // Handle lists specially. We don't want to call this.set()
          // unconditionally upon updating a list value, since even its contents
          // are the same as the old list, doing this set() may cause an
          // infinite update cycle (http://crbug.com/498586).
          if (objKey == 'value' &&
              prefObj.type == chrome.settingsPrivate.PrefType.LIST &&
              !this.shouldUpdateListPrefValue_(root, prefObj['value'])) {
            continue;
          }

          this.set(path, prefObj[objKey]);
        }

        if (shouldObserve) {
          Object.observe(root, this.propertyChangeCallback_, ['update']);
        }
      }, this);
    },


    /**
     * @param {Object} root The root object for a pref that contains a list
     *     value.
     * @param {!Array} newValue The new list value.
     * @return {boolean} Whether the new value is different from the one in
     *     root, thus necessitating a pref update.
     */
    shouldUpdateListPrefValue_: function(root, newValue) {
      if (root.value == null ||
          root.value.length != newValue.length) {
        return true;
      }

      for (let i = 0; i < newValue.length; i++) {
        if (root.value != null && root.value[i] != newValue[i]) {
          return true;
        }
      }

      return false;
    },

    /**
     * Called when a property of a pref changes.
     * @param {!Array<!Object>} changes An array of objects describing changes.
     *     @see http://www.html5rocks.com/en/tutorials/es7/observe/
     * @private
     */
    propertyChangeCallback_: function(changes) {
      changes.forEach(function(change) {
        // UI should only be able to change the value of a setting for now, not
        // disabled, etc.
        assert(change.name == 'value');

        let newValue = change.object[change.name];
        assert(newValue !== undefined);

        chrome.settingsPrivate.setPref(
            change.object['key'],
            newValue,
            /* pageId */ '',
            /* callback */ function() {});
      });
    },
  });
})();
