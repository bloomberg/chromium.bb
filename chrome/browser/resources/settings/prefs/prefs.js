/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * @fileoverview
 * 'cr-settings-prefs' models Chrome settings and preferences, listening for
 * changes to Chrome prefs whitelisted in chrome.settingsPrivate.
 * When changing prefs in this element's 'prefs' property via the UI, this
 * element tries to set those preferences in Chrome. Whether or not the calls to
 * settingsPrivate.setPref succeed, 'prefs' is eventually consistent with the
 * Chrome pref store.
 *
 * Example:
 *
 *    <cr-settings-prefs prefs="{{prefs}}"></cr-settings-prefs>
 *    <cr-settings-a11y-page prefs="{{prefs}}"></cr-settings-a11y-page>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-prefs
 */

/**
 * Pref state object. Copies values of PrefObjects received from
 * settingsPrivate to determine when pref values have changed.
 * This prototype works for primitive types, but more complex types should
 * override these functions.
 * @constructor
 * @param {!chrome.settingsPrivate.PrefObject} prefObj
 */
function PrefWrapper(prefObj) {
  this.key_ = prefObj.key;
  this.value_ = prefObj.value;
}

/**
 * Checks if other's value equals this.value_. Both prefs wrapped by the
 * PrefWrappers should have the same keys.
 * @param {PrefWrapper} other
 * @return {boolean}
 */
PrefWrapper.prototype.equals = function(other) {
  assert(this.key_ == other.key_);
  return this.value_ == other.value_;
};

/**
 * @constructor
 * @extends {PrefWrapper}
 * @param {!chrome.settingsPrivate.PrefObject} prefObj
 */
function ListPrefWrapper(prefObj) {
  // Copy the array so changes to prefObj aren't reflected in this.value_.
  // TODO(michaelpg): Do a deep copy to support nested lists and objects.
  this.value_ = prefObj.value.slice();
}

  ListPrefWrapper.prototype = {
  __proto__: PrefWrapper.prototype,

  /**
   * Tests whether two ListPrefWrapper values contain the same list items.
   * @override
   */
  equals: function(other) {
    'use strict';
    assert(this.key_ == other.key_);
    if (this.value_.length != other.value_.length)
      return false;
    for (let i = 0; i < this.value_.length; i++) {
      if (this.value_[i] != other.value_[i])
        return false;
    }
    return true;
  },
};

(function() {
  'use strict';

  Polymer({
    is: 'cr-settings-prefs',

    properties: {
      /**
       * Object containing all preferences, for use by Polymer controls.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Map of pref keys to PrefWrapper objects representing the state of the
       * Chrome pref store.
       * @type {Object<PrefWrapper>}
       * @private
       */
      prefWrappers_: {
        type: Object,
        value: function() { return {}; },
      },
    },

    observers: [
      'prefsChanged_(prefs.*)',
    ],

    /** @override */
    created: function() {
      CrSettingsPrefs.isInitialized = false;

      chrome.settingsPrivate.onPrefsChanged.addListener(
          this.onSettingsPrivatePrefsChanged_.bind(this));
      chrome.settingsPrivate.getAllPrefs(
          this.onSettingsPrivatePrefsFetched_.bind(this));
    },

    /**
     * Polymer callback for changes to this.prefs.
     * @param {!{path: string, value: *}} change
     * @private
     */
    prefsChanged_: function(change) {
      if (!CrSettingsPrefs.isInitialized)
        return;

      var key = this.getPrefKeyFromPath_(change.path);
      var prefWrapper = this.prefWrappers_[key];
      if (!prefWrapper)
        return;

      var prefObj = /** @type {chrome.settingsPrivate.PrefObject} */(
          this.get(key, this.prefs));

      // If settingsPrivate already has this value, do nothing. (Otherwise,
      // a change event from settingsPrivate could make us call
      // settingsPrivate.setPref and potentially trigger an IPC loop.)
      if (prefWrapper.equals(this.createPrefWrapper_(prefObj)))
        return;

      chrome.settingsPrivate.setPref(
          key,
          prefObj.value,
          /* pageId */ '',
          /* callback */ this.setPrefCallback_.bind(this, key));
    },

    /**
     * Called when prefs in the underlying Chrome pref store are changed.
     * @param {!Array<!chrome.settingsPrivate.PrefObject>} prefs
     *     The prefs that changed.
     * @private
     */
    onSettingsPrivatePrefsChanged_: function(prefs) {
      if (CrSettingsPrefs.isInitialized)
        this.updatePrefs_(prefs);
    },

    /**
     * Called when prefs are fetched from settingsPrivate.
     * @param {!Array<!chrome.settingsPrivate.PrefObject>} prefs
     * @private
     */
    onSettingsPrivatePrefsFetched_: function(prefs) {
      this.updatePrefs_(prefs);

      CrSettingsPrefs.isInitialized = true;
      document.dispatchEvent(new Event(CrSettingsPrefs.INITIALIZED));
    },

    /**
     * Checks the result of calling settingsPrivate.setPref.
     * @param {string} key The key used in the call to setPref.
     * @param {boolean} success True if setting the pref succeeded.
     * @private
     */
    setPrefCallback_: function(key, success) {
      if (success)
        return;

      // Get the current pref value from chrome.settingsPrivate to ensure the
      // UI stays up to date.
      chrome.settingsPrivate.getPref(key, function(pref) {
        this.updatePrefs_([pref]);
      }.bind(this));
    },

    /**
     * Updates the prefs model with the given prefs.
     * @param {!Array<!chrome.settingsPrivate.PrefObject>} newPrefs
     * @private
     */
    updatePrefs_: function(newPrefs) {
      // Use the existing prefs object or create it.
      var prefs = this.prefs || {};
      newPrefs.forEach(function(newPrefObj) {
        // Use the PrefObject from settingsPrivate to create a PrefWrapper in
        // prefWrappers_ at the pref's key.
        this.prefWrappers_[newPrefObj.key] =
            this.createPrefWrapper_(newPrefObj);

        // Add the pref to |prefs|.
        cr.exportPath(newPrefObj.key, newPrefObj, prefs);
        // If this.prefs already exists, notify listeners of the change.
        if (prefs == this.prefs)
          this.notifyPath('prefs.' + newPrefObj.key, newPrefObj);
      }, this);
      if (!this.prefs)
        this.prefs = prefs;
    },

    /**
     * Given a 'property-changed' path, returns the key of the preference the
     * path refers to. E.g., if the path of the changed property is
     * 'prefs.search.suggest_enabled.value', the key of the pref that changed is
     * 'search.suggest_enabled'.
     * @param {string} path
     * @return {string}
     * @private
     */
    getPrefKeyFromPath_: function(path) {
      // Skip the first token, which refers to the member variable (this.prefs).
      var parts = path.split('.');
      assert(parts.shift() == 'prefs');

      for (let i = 1; i <= parts.length; i++) {
        let key = parts.slice(0, i).join('.');
        // The prefWrappers_ keys match the pref keys.
        if (this.prefWrappers_[key] != undefined)
          return key;
      }
      return '';
    },

    /**
     * Creates a PrefWrapper object from a chrome.settingsPrivate pref.
     * @param {!chrome.settingsPrivate.PrefObject} prefObj
     *     PrefObject received from chrome.settingsPrivate.
     * @return {PrefWrapper} An object containing a copy of the PrefObject's
     *     value.
     * @private
     */
    createPrefWrapper_: function(prefObj) {
      return prefObj.type == chrome.settingsPrivate.PrefType.LIST ?
          new ListPrefWrapper(prefObj) : new PrefWrapper(prefObj);
    },
  });
})();
