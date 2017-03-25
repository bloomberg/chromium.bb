// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to manage user preferences.
 *
 * @constructor
 */
let SwitchAccessPrefs = function() {
  /**
   * User preferences, initially set to the default preference values.
   *
   * @private
   */
  this.prefs_ = Object.assign({}, this.DEFAULT_PREFS);
  this.loadPrefs_();
  chrome.storage.onChanged.addListener(this.handleStorageChange_.bind(this));
};

SwitchAccessPrefs.prototype = {
  /**
   * Asynchronously load the current preferences from chrome.storage.sync and
   * store them in this.prefs_.
   *
   * @private
   */
  loadPrefs_: function() {
    let defaultKeys = Object.keys(this.DEFAULT_PREFS);
    chrome.storage.sync.get(defaultKeys, function(loadedPrefs) {
      let loadedKeys = Object.keys(loadedPrefs);
      if (loadedKeys.length > 0) {
        for (let key of loadedKeys) {
          this.prefs_[key] = loadedPrefs[key];
        }
        let event = new CustomEvent('prefsUpdate', {'detail': loadedPrefs});
        document.dispatchEvent(event);
      }
    }.bind(this));
  },

  /**
   * Store any changes to chrome.storage.sync in this.prefs_.
   *
   * @param {!Object} changes
   * @param {string} areaName
   * @private
   */
  handleStorageChange_: function(changes, areaName) {
    for (let key of Object.keys(changes)) {
      // If pref change happened on same device, prefs will already be updated,
      // so this will have no effect.
      this.prefs_[key] = changes[key].newValue;
      changes[key] = changes[key].newValue;
    }

    let event = new CustomEvent('prefsUpdate', {'detail': changes});
    document.dispatchEvent(event);
  },

  /**
   * Set the value of the preference |key| to |value|.
   *
   * @param {string} key
   * @param {boolean|string|number} value
   */
  setPref: function(key, value) {
    let pref = {};
    pref[key] = value;
    chrome.storage.sync.set(pref);
    this.prefs_[key] = value;
  },

  /**
   * Get all user preferences.
   */
  getPrefs: function() {
    return Object.assign({}, this.prefs_);
  },

  /**
   * The default value of all preferences. All preferences should be primitives
   * to prevent changes to default values.
   *
   * @const
   */
  DEFAULT_PREFS: {
    'enableAutoScan': false,
    'autoScanTime': .75
  }
};
