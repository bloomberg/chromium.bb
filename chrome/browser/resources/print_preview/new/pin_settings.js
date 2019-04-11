// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-pin-settings',

  behaviors: [SettingsBehavior, print_preview_new.InputBehavior],

  properties: {
    disabled: Boolean,

    /** @private {boolean} */
    checkboxDisabled_: {
      type: Boolean,
      computed: 'computeCheckboxDisabled_(inputValid_, disabled, ' +
          'settings.pin.setByPolicy)',
    },

    /** @private {boolean} */
    pinEnabled_: {
      type: Boolean,
      value: false,
    },

    /** @private {string} */
    inputString_: {
      type: String,
      value: '',
      observer: 'onInputChanged_',
    },

    /** @private */
    inputValid_: {
      type: Boolean,
      value: true,
      reflectToAttribute: true,
    },
  },

  observers:
      ['onSettingsChanged_(settings.pin.value, settings.pinValue.value)'],

  listeners: {
    'input-change': 'onInputChange_',
  },

  /** @return {!CrInputElement} The cr-input field element for InputBehavior. */
  getInput: function() {
    return this.$.pinValue;
  },

  /**
   * @param {!CustomEvent<string>} e Contains the new input value.
   * @private
   */
  onInputChange_: function(e) {
    this.inputString_ = e.detail;
  },

  /** @private */
  onCollapseChanged_: function() {
    if (this.pinEnabled_) {
      /** @type {!CrInputElement} */ (this.$.pinValue).inputElement.focus();
    }
  },

  /**
   * @param {boolean} inputValid Whether pin value is valid.
   * @param {boolean} disabled Whether pin setting is disabled.
   * @param {boolean} managed Whether pin setting is managed.
   * @return {boolean} Whether pin checkbox should be disabled.
   * @private
   */
  computeCheckboxDisabled_: function(inputValid, disabled, managed) {
    return inputValid && (disabled || managed);
  },

  /**
   * @return {boolean} Whether to disable the pin value input.
   * @private
   */
  inputDisabled_: function() {
    return !this.pinEnabled_ || (this.inputValid_ && this.disabled);
  },

  /**
   * Updates the checkbox state when the setting has been initialized.
   * @private
   */
  onSettingsChanged_: function() {
    const pinEnabled = /** @type {boolean} */ (this.getSetting('pin').value);
    this.$.pin.checked = pinEnabled;
    this.pinEnabled_ = pinEnabled;
    const pinValue = this.getSetting('pinValue');
    this.inputString_ = /** @type {string} */ (pinValue.value);
  },

  /** @private */
  onPinChange_: function() {
    this.setSetting('pin', this.$.pin.checked);
  },

  /**
   * Updates pin value setting based on the current value of the pin value
   * input.
   * @private
   */
  onInputChanged_: function() {
    if (this.settings === undefined) {
      return;
    }
    this.inputValid_ = this.computeValid_();
    this.setSettingValid('pinValue', this.inputValid_);

    if (this.inputValid_) {
      this.setSetting('pinValue', this.inputString_);
    }
  },

  /**
   * @return {boolean} Whether input value represented by inputString_ is
   *     valid, so that it can be used to update the setting.
   * @private
   */
  computeValid_: function() {
    // Make sure value updates first, in case inputString_ was updated by JS.
    this.$.pinValue.value = this.inputString_;
    return !this.$.pinValue.invalid;
  },
});
