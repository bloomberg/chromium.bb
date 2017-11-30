// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @polymerBehavior */
const SettingsBehavior = {
  properties: {
    /** @type {Object} */
    settings: {
      type: Object,
      notify: true,
    },
  },

  /**
   * @param {string} settingName Name of the setting to get.
   * @return {print_preview_new.Setting} The setting object.
   */
  getSetting: function(settingName) {
    const setting = /** @type {print_preview_new.Setting} */ (
        this.get(settingName, this.settings));
    assert(!!setting, 'Setting is missing: ' + settingName);
    return setting;
  },

  /**
   * @param {string} settingName Name of the setting to set
   * @param {boolean | string | number | Array | Object} value The value to set
   *     the setting to.
   */
  setSetting: function(settingName, value) {
    const setting = this.getSetting(settingName);
    this.set(`settings.${settingName}.value`, value);
  },

  /**
   * @param {string} settingName Name of the setting to set
   * @param {boolean} valid Whether the setting value is currently valid.
   */
  setSettingValid: function(settingName, valid) {
    const setting = this.getSetting(settingName);
    // Should not set the setting to invalid if it is not available, as there
    // is no way for the user to change the value in this case.
    if (!valid)
      assert(setting.available, 'Setting is not available: ' + settingName);
    this.set(`settings.${settingName}.valid`, valid);
  }
};
