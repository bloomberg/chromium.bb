// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-advanced-settings-item',

  properties: {
    /** @type {!print_preview.VendorCapability} */
    capability: Object,

    /** @type {?RegExp} */
    searchQuery: Object,

    /** @private {(number | string | boolean)} */
    currentValue_: {
      type: Object,
      value: null,
    },
  },

  /** @private */
  onSelectChange_: function() {
    this.currentValue_ = this.$$('select').value;
  },

  /**
   * @param {!print_preview.VendorCapability |
   *         !print_preview.VendorCapabilitySelectOption} item
   * @return {string} The display name for the setting.
   * @private
   */
  getDisplayName_: function(item) {
    let displayName = item.display_name;
    if (!displayName && item.display_name_localized)
      displayName = getStringForCurrentLocale(item.display_name_localized);
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
    return (this.currentValue_ !== null &&
            option.value === this.currentValue_) ||
        (this.currentValue_ == null && !!option.is_default);
  },

  /**
   * @return {string} The placeholder value for the capability's text input.
   * @private
   */
  getCapabilityPlaceholder_: function() {
    if (this.capability.type == 'TYPED_VALUE' &&
        this.capability.typed_value_cap) {
      return this.capability.typed_value_cap.default.toString() || '';
    }
    if (this.capability.type == 'RANGE' && this.capability.range_type)
      return this.capability.range_cap.default.toString() || '';
    return '';
  },
});
