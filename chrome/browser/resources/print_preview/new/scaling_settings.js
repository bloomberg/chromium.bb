// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview_new');

/** @enum {number} */
const ScalingValue = {
  DEFAULT: 0,
  FIT_TO_PAGE: 1,
  CUSTOM: 2,
};

/*
 * When fit to page is available, the checkbox and input interact as follows:
 * 1. When checkbox is checked, the fit to page scaling value is displayed in
 * the input. The error message is cleared if it was present.
 * 2. When checkbox is unchecked, the most recent valid scale value is restored.
 * 3. If the input is modified while the checkbox is checked, the checkbox will
 * be unchecked automatically, regardless of the validity of the new value.
 */
Polymer({
  is: 'print-preview-scaling-settings',

  behaviors: [SettingsBehavior, print_preview_new.SelectBehavior],

  properties: {
    disabled: Boolean,

    /** @private {string} */
    currentValue_: {
      type: String,
      observer: 'onInputChanged_',
    },

    /** @private {boolean} */
    customSelected_: {
      type: Boolean,
      computed: 'computeCustomSelected_(selectedValue)',
    },

    /** @private {boolean} */
    inputValid_: Boolean,

    /**
     * Mirroring the enum so that it can be used from HTML bindings.
     * @private
     */
    scalingValueEnum_: {
      type: Object,
      value: ScalingValue,
    },
  },

  observers: [
    'onFitToPageSettingChange_(settings.fitToPage.value)',
    'onScalingSettingChanged_(settings.scaling.value)',
    'onCustomScalingSettingChanged_(settings.customScaling.value)',
  ],

  /** @private {string} */
  lastValidScaling_: '',

  onProcessSelectChange: function(value) {
    if (value === ScalingValue.FIT_TO_PAGE.toString()) {
      this.setSetting('fitToPage', true);
      return;
    }

    const fitToPageAvailable = this.getSetting('fitToPage').available;
    if (fitToPageAvailable) {
      this.setSetting('fitToPage', false);
    }
    const isCustom = value === ScalingValue.CUSTOM.toString();
    this.setSetting('customScaling', isCustom);
    if (isCustom) {
      this.setSetting('scaling', this.currentValue_);
    }
  },

  /** @private */
  updateScalingToValid_: function() {
    if (!this.getSetting('scaling').valid) {
      this.currentValue_ = this.lastValidScaling_;
    } else {
      this.lastValidScaling_ = this.currentValue_;
    }
  },

  /** @private */
  onFitToPageSettingChange_: function() {
    if (!this.getSettingValue('fitToPage') ||
        !this.getSetting('fitToPage').available) {
      return;
    }

    this.updateScalingToValid_();
    this.selectedValue = ScalingValue.FIT_TO_PAGE.toString();
  },

  /** @private */
  onCustomScalingSettingChanged_: function() {
    if (this.getSettingValue('fitToPage') &&
        this.getSetting('fitToPage').available) {
      return;
    }

    const isCustom =
        /** @type {boolean} */ (this.getSetting('customScaling').value);
    if (!isCustom) {
      this.updateScalingToValid_();
    }
    this.selectedValue = isCustom ? ScalingValue.CUSTOM.toString() :
                                    ScalingValue.DEFAULT.toString();
  },

  /**
   * Updates the input string when scaling setting is set.
   * @private
   */
  onScalingSettingChanged_: function() {
    const value = /** @type {string} */ (this.getSetting('scaling').value);
    this.lastValidScaling_ = value;
    this.currentValue_ = value;
  },

  /**
   * Updates scaling and fit to page settings based on the validity and current
   * value of the scaling input.
   * @private
   */
  onInputChanged_: function() {
    if (this.currentValue_ !== '') {
      this.setSettingValid('scaling', this.inputValid_);
    }

    if (this.currentValue_ !== '' && this.inputValid_) {
      this.setSetting('scaling', this.currentValue_);
    }
  },

  /**
   * @return {boolean} Whether the dropdown should be disabled.
   * @private
   */
  dropdownDisabled_: function() {
    return this.disabled && this.inputValid_;
  },

  /**
   * @return {boolean} Whether the input should be disabled.
   * @private
   */
  inputDisabled_: function() {
    return !this.customSelected_ || (this.disabled && this.inputValid_);
  },

  /**
   * @return {boolean} Whether the custom scaling option is selected.
   * @private
   */
  computeCustomSelected_: function() {
    return this.selectedValue === ScalingValue.CUSTOM.toString();
  },

  /** @private */
  onCollapseChanged_: function() {
    if (this.customSelected_) {
      this.$$('print-preview-number-settings-section').getInput().focus();
    }
  },
});
