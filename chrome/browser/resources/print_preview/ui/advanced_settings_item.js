// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-advanced-settings-item',

  behaviors: [SettingsBehavior],

  properties: {
    /** @type {!print_preview.VendorCapability} */
    capability: Object,

    /** @private {string} */
    currentValue_: String,
  },

  observers: [
    'updateFromSettings_(capability, settings.vendorItems.value)',
  ],

  /** @private */
  updateFromSettings_: function() {
    const settings = this.getSetting('vendorItems').value;

    // The settings may not have a property with the id if they were populated
    // from sticky settings from a different destination or if the
    // destination's capabilities changed since the sticky settings were
    // generated.
    if (!settings.hasOwnProperty(this.capability.id)) {
      return;
    }

    const value = settings[this.capability.id];
    if (this.isCapabilityTypeSelect_()) {
      // Ignore a value that can't be selected.
      if (this.hasOptionWithValue_(value)) {
        this.currentValue_ = value;
      }
    } else {
      this.currentValue_ = value;
      this.$$('cr-input').value = this.currentValue_;
    }
  },

  /**
   * @param {!print_preview.VendorCapability |
   *         !print_preview.VendorCapabilitySelectOption} item
   * @return {string} The display name for the setting.
   * @private
   */
  getDisplayName_: function(item) {
    let displayName = item.display_name;
    if (!displayName && item.display_name_localized) {
      displayName = getStringForCurrentLocale(item.display_name_localized);
    }
    return displayName || '';
  },

  /**
   * @return {boolean} Whether the capability represented by this item is
   *     of type select.
   * @private
   */
  isCapabilityTypeSelect_: function() {
    return this.capability.type == 'SELECT';
  },

  /**
   * @param {!print_preview.VendorCapabilitySelectOption} option The option
   *     for a select capability.
   * @return {boolean} Whether the option is selected.
   * @private
   */
  isOptionSelected_: function(option) {
    return this.currentValue_ === undefined ?
        !!option.is_default :
        option.value === this.currentValue_;
  },

  /**
   * @return {string} The placeholder value for the capability's text input.
   * @private
   */
  getCapabilityPlaceholder_: function() {
    if (this.capability.type == 'TYPED_VALUE' &&
        this.capability.typed_value_cap &&
        this.capability.typed_value_cap.default != undefined) {
      return this.capability.typed_value_cap.default.toString() || '';
    }
    if (this.capability.type == 'RANGE' && this.capability.range_cap &&
        this.capability.range_cap.default != undefined) {
      return this.capability.range_cap.default.toString() || '';
    }
    return '';
  },

  /**
   * @return {boolean}
   * @private
   */
  hasOptionWithValue_: function(value) {
    return !!this.capability.select_cap &&
        !!this.capability.select_cap.option &&
        this.capability.select_cap.option.some(option => option.value == value);
  },

  /**
   * @param {?RegExp} query The current search query.
   * @return {boolean} Whether the item has a match for the query.
   */
  hasMatch: function(query) {
    if (!query || this.getDisplayName_(this.capability).match(query)) {
      return true;
    }

    if (!this.isCapabilityTypeSelect_()) {
      return false;
    }

    for (const option of
         /** @type {!Array<!print_preview.VendorCapabilitySelectOption>} */ (
             this.capability.select_cap.option)) {
      if (this.getDisplayName_(option).match(query)) {
        return true;
      }
    }
    return false;
  },

  /**
   * @param {!Event} e Event containing the new value.
   * @private
   */
  onUserInput_: function(e) {
    this.currentValue_ = e.target.value;
  },

  /**
   * @return {string} The current value of the setting, or the empty string if
   *     it is not set.
   */
  getCurrentValue: function() {
    return this.currentValue_ || '';
  },

  /**
   * Only used in tests.
   * @param {string} value A value to set the setting to.
   */
  setCurrentValueForTest: function(value) {
    this.currentValue_ = value;
  },

  /**
   * @param {?RegExp} query The current search query.
   * @return {!print_preview.HighlightResults} The highlight wrappers and
   *     search bubbles that were created.
   */
  updateHighlighting: function(query) {
    return print_preview.updateHighlights(this, query);
  },
});
