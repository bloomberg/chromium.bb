// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-copies-settings',

  properties: {
    /** @type {!print_preview_new.Model} */
    model: {
      type: Object,
      notify: true,
    },

    /** @private {string} */
    inputString_: String,

    /** @private {boolean} */
    inputValid_: Boolean,
  },

  observers: ['onCopiesChanged_(inputString_, inputValid_)'],

  /**
   * Updates model.copies and model.copiesInvalid based on the validity
   * and current value of the copies input.
   * @private
   */
  onCopiesChanged_: function() {
    this.set(
        'model.copies', this.inputValid_ ? parseInt(this.inputString_, 10) : 1);
    this.set('model.copiesInvalid', !this.inputValid_);
  },

  /**
   * @return {boolean} Whether collate checkbox should be hidden.
   * @private
   */
  collateHidden_: function() {
    return !this.inputValid_ || parseInt(this.inputString_, 10) == 1;
  },
});
