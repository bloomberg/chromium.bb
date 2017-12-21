// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview_new');
/**
 * @typedef {{
 *   is_default: (boolean | undefined),
 *   custom_display_name: (string | undefined),
 *   custom_display_name_localized: (Array<!{locale: string, value:string}> |
 *                                   undefined),
 *   name: (string | undefined),
 * }}
 */
print_preview_new.SelectOption;

Polymer({
  is: 'print-preview-settings-select',

  behaviors: [SettingsBehavior],

  properties: {
    /** @type {{ option: Array<!print_preview_new.SelectOption> }} */
    capability: Object,

    /** @type {string} */
    settingName: String,
  },

  /** @param {string} value The value to select. */
  selectValue: function(value) {
    this.$$('select').value = value;
  },

  /**
   * @param {!print_preview_new.SelectOption} option Option to get the value
   *    for.
   * @return {string} Value for the option.
   * @private
   */
  getValue_: function(option) {
    return JSON.stringify(option);
  },

  /**
   * @param {!print_preview_new.SelectOption} option Option to get the display
   *    name for.
   * @return {string} Display name for the option.
   * @private
   */
  getDisplayName_: function(option) {
    let displayName = option.custom_display_name;
    if (!displayName && option.custom_display_name_localized) {
      displayName = getStringForCurrentLocale(
          assert(option.custom_display_name_localized));
    }
    return displayName || option.name || '';
  },

  /** @private */
  onChange_: function() {
    let value = null;
    try {
      value = JSON.parse(this.$$('select').value);
    } catch (e) {
      assertNotReached();
      return;
    }
    this.setSetting(this.settingName, /** @type {Object} */ (value));
  },
});
