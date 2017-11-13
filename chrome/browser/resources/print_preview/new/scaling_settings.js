// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-scaling-settings',

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

  /** @private {string} */
  lastValidScaling_: '100',

  /** @private {number} */
  fitToPageFlag_: 0,

  observers: ['onScalingChanged_(inputString_, inputValid_)'],

  /**
   * Updates model.scaling and model.scalingInvalid based on the validity
   * and current value of the scaling input.
   * @private
   */
  onScalingChanged_: function() {
    if (this.fitToPageFlag_ > 0) {
      this.fitToPageFlag_--;
      return;
    }
    const checkbox = this.$$('.checkbox input[type="checkbox"]');
    if (checkbox.checked && this.model.isPdfDocument) {
      checkbox.checked = false;
    } else if (this.inputValid_) {
      this.lastValidScaling_ = this.inputString_;
    }
    this.set('model.scalingInvalid', !this.inputValid_);
  },

  /**
   * Updates scaling as needed based on the value of the fit to page checkbox.
   */
  onFitToPageChange_: function() {
    if (this.$$('.checkbox input[type="checkbox"]').checked) {
      this.fitToPageFlag_ = 2;
      this.set('inputString_', this.model.fitToPageScaling);
    } else {
      this.set('inputString_', this.lastValidScaling_);
    }
  },
});
