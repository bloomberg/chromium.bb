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
    this.preferences_ = Object.assign({}, SAConstants.DEFAULT_PREFERENCES);

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
    for (const name of Object.keys(storageChanges)) {
      if (this.preferences_[name] !== storageChanges[name].newValue) {
        this.preferences_[name] = storageChanges[name].newValue;
        updatedPreferences[name] = storageChanges[name].newValue;
      }
    }
    if (Object.keys(updatedPreferences).length > 0)
      this.switchAccess_.onPreferencesChanged(updatedPreferences);
  }

  /**
   * @private
   */
  init_() {
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
    const defaultKeys = Object.values(SAConstants.Preference);
    chrome.storage.sync.get(defaultKeys, (loadedPreferences) => {
      let updatedPreferences = {};

      for (const name of Object.keys(loadedPreferences)) {
        if (this.preferences_[name] !== loadedPreferences[name]) {
          this.preferences_[name] = loadedPreferences[name];
          updatedPreferences[name] = loadedPreferences[name];
        }
      }

      if (Object.keys(updatedPreferences).length > 0)
        this.switchAccess_.onPreferencesChanged(updatedPreferences);
    });
  }

  /**
   * Set the value of the preference |name| to |value| in |chrome.storage.sync|.
   * |this.preferences_| is not set until |handleStorageChange_|.
   *
   * @param {SAConstants.Preference} name
   * @param {boolean|number} value
   */
  setPreference(name, value) {
    let preference = {};
    preference[name] = value;
    chrome.storage.sync.set(preference);
  }

  /**
   * Get the boolean value for the given name. Will throw a type error if the
   * value associated with |name| is not a boolean, or undefined.
   *
   * @param  {SAConstants.Preference} name
   * @return {boolean}
   */
  getBooleanPreference(name) {
    const value = this.preferences_[name];
    if (typeof value === 'boolean')
      return value;
    else
      throw new TypeError('No value of boolean type named \'' + name + '\'');
  }

  /**
   * Get the number value for the given name. Will throw a type error if the
   * value associated with |name| is not a number, or undefined.
   *
   * @param  {SAConstants.Preference} name
   * @return {number}
   */
  getNumberPreference(name) {
    const value = this.getNumberPreferenceIfDefined(name);
    if (!value)
      throw new TypeError('No value of number type named \'' + name + '\'');
    return value;
  }

  /**
   * Get the number value for the given name, or |null| if the value is not a
   * number or does not exist.
   *
   * @param {SAConstants.Preference} name
   * @return {number|null}
   */
  getNumberPreferenceIfDefined(name) {
    const value = this.preferences_[name];
    if (typeof value === 'number')
      return value;
    return null;
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

