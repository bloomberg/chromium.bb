// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');
/**
 * @typedef {{
 *   is_default: (boolean | undefined),
 *   custom_display_name: (string | undefined),
 *   custom_display_name_localized: (Array<!{locale: string, value:string}> |
 *                                   undefined),
 *   name: (string | undefined),
 * }}
 */
print_preview.SelectOption;

Polymer({
  is: 'print-preview-settings-select',

  behaviors: [SettingsBehavior, print_preview.SelectBehavior],

  properties: {
    /** @type {{ option: Array<!print_preview.SelectOption> }} */
    capability: Object,

    settingName: String,

    disabled: Boolean,
  },

  /**
   * @param {!print_preview.SelectOption} option Option to check.
   * @return {boolean} Whether the option is selected.
   * @private
   */
  isSelected_: function(option) {
    return this.getValue_(option) == this.selectedValue ||
        (!!option.is_default && this.selectedValue == '');
  },

  /** @param {string} value The value to select. */
  selectValue: function(value) {
    this.selectedValue = value;
  },

  /**
   * @param {!print_preview.SelectOption} option Option to get the value
   *    for.
   * @return {string} Value for the option.
   * @private
   */
  getValue_: function(option) {
    return JSON.stringify(option);
  },

  /**
   * @param {!print_preview.SelectOption} option Option to get the display
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

  /** @param {string} value The new select value. */
  onProcessSelectChange: function(value) {
    let newValue = null;
    try {
      newValue = JSON.parse(value);
    } catch (e) {
      assertNotReached();
      return;
    }
    if (value !== JSON.stringify(this.getSettingValue(this.settingName))) {
      this.setSetting(this.settingName, /** @type {Object} */ (newValue));
    }
  },
});
