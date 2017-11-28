// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-copies-settings',

  behaviors: [SettingsBehavior],

  properties: {
    /** @private {string} */
    inputString_: String,

    /** @private {boolean} */
    inputValid_: Boolean,
  },

  /** @private {boolean} */
  isInitialized_: false,

  observers: [
    'onInputChanged_(inputString_, inputValid_)',
    'onInitialized_(settings.copies.value)'
  ],

  /**
   * Updates the input string when the setting has been initialized.
   * @private
   */
  onInitialized_: function() {
    if (this.isInitialized_)
      return;
    this.isInitialized_ = true;
    const copies = this.getSetting('copies');
    this.set('inputString_', copies.value);
  },

  /**
   * Updates model.copies and model.copiesInvalid based on the validity
   * and current value of the copies input.
   * @private
   */
  onInputChanged_: function() {
    this.setSetting(
        'copies', this.inputValid_ ? parseInt(this.inputString_, 10) : 1);
    this.setSettingValid('copies', this.inputValid_);
  },

  /**
   * @return {boolean} Whether collate checkbox should be hidden.
   * @private
   */
  collateHidden_: function() {
    return !this.inputValid_ || parseInt(this.inputString_, 10) == 1;
  },
});
