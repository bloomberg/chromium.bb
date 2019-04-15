// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to manage user preferences.
 */
class SwitchAccessPreferences {
  /**
   * @param {!SwitchAccessInterface} switchAccess
   */
  constructor(switchAccess) {
    /**
     * SwitchAccess reference.
     * @private {!SwitchAccessInterface}
     */
    this.switchAccess_ = switchAccess;

    /**
     * User preferences, initially set to the default preference values.
     * @private
     */
    this.preferences_ = Object.assign({}, DEFAULT_PREFERENCES);

    this.init_();
  }

  /**
   * Store any changes from |chrome.storage.sync| to |this.preferences_|, if
   * |this.preferences_| is not already set to that value.
   *
   * @param {!Object} storageChanges
   * @param {string} areaName
   * @private
   */
  handleStorageChange_(storageChanges, areaName) {
    let updatedPreferences = {};
    for (const key of Object.keys(storageChanges)) {
      if (this.preferences_[key] !== storageChanges[key].newValue) {
        this.preferences_[key] = storageChanges[key].newValue;
        updatedPreferences[key] = storageChanges[key].newValue;
      }
    }
    if (Object.keys(updatedPreferences).length > 0)
      this.switchAccess_.onPreferencesChanged(updatedPreferences);
  }

  /**
   * @private
   */
  init_() {
    for (const command of this.switchAccess_.getCommands())
      this.preferences_[command] =
          this.switchAccess_.getDefaultKeyCodeFor(command);

    this.loadPreferences_();
    chrome.storage.onChanged.addListener(this.handleStorageChange_.bind(this));
  }

  /**
   * Asynchronously load the current preferences from |chrome.storage.sync| and
   * store them in |this.preferences_|, if |this.preferences_| is not already
   * set to that value.
   *
   * @private
   */
  loadPreferences_() {
    const defaultKeys = Object.keys(this.preferences_);
    chrome.storage.sync.get(defaultKeys, (loadedPreferences) => {
      let updatedPreferences = {};

      for (const key of Object.keys(loadedPreferences)) {
        if (this.preferences_[key] !== loadedPreferences[key]) {
          this.preferences_[key] = loadedPreferences[key];
          updatedPreferences[key] = loadedPreferences[key];
        }
      }

      if (Object.keys(updatedPreferences).length > 0)
        this.switchAccess_.onPreferencesChanged(updatedPreferences);
    });
  }

  /**
   * Set the value of the preference |key| to |value| in |chrome.storage.sync|.
   * |this.preferences_| is not set until |handleStorageChange_|.
   *
   * @param {string} key
   * @param {boolean|string|number} value
   */
  setPreference(key, value) {
    let preference = {};
    preference[key] = value;
    chrome.storage.sync.set(preference);
  }

  /**
   * Get the value of type 'boolean' of the preference |key|. Will throw a type
   * error if the value of |key| is not 'boolean'.
   *
   * @param  {string} key
   * @return {boolean}
   */
  getBooleanPreference(key) {
    const value = this.preferences_[key];
    if (typeof value === 'boolean')
      return value;
    else
      throw new TypeError('No value of boolean type for key \'' + key + '\'');
  }

  /**
   * Get the value of type 'number' of the preference |key|. Will throw a type
   * error if the value of |key| is not 'number'.
   *
   * @param  {string} key
   * @return {number}
   */
  getNumberPreference(key) {
    const value = this.preferences_[key];
    if (typeof value === 'number')
      return value;
    else
      throw new TypeError('No value of number type for key \'' + key + '\'');
  }

  /**
   * Returns true if |keyCode| is already used to run a command from the
   * keyboard.
   *
   * @param {number} keyCode
   * @return {boolean}
   */
  keyCodeIsUsed(keyCode) {
    for (const command of this.switchAccess_.getCommands()) {
      if (keyCode === this.preferences_[command])
        return true;
    }
    return false;
  }
}

/**
 * The default value of all preferences besides command keyboard bindings.
 * All preferences should be primitives to prevent changes to default values.
 */
const DEFAULT_PREFERENCES = {
  'enableAutoScan': false,
  'autoScanTime': 800
};
