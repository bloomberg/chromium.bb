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
    copiesString_: {
      type: String,
      value: '1',
    },

    /** @private {boolean} */
    copiesValid_: {
      type: Boolean,
      computed: 'computeCopiesValid_(copiesString_)',
    },
  },

  observers: ['onCopiesChanged_(copiesString_, copiesValid_)'],

  /**
   * @param {!KeyboardEvent} e The keyboard event
   * @private
   */
  onCopiesKeydown_: function(e) {
    if (e.key == '.' || e.key == 'e' || e.key == '-')
      e.preventDefault();
  },

  /** @private */
  onCopiesBlur_: function() {
    if (this.copiesString_ == '') {
      this.$$('.user-value').value = '1';
      this.set('copiesString_', '1');
    }
  },

  /**
   * @return {boolean} Whether copies value represented by copiesString_ is
   *     valid.
   * @private
   */
  computeCopiesValid_: function() {
    return this.$$('.user-value').validity.valid && this.copiesString_ != '';
  },

  /**
   * Updates model.copies and model.printTicketInvalid based on the validity
   * and current value of the copies input.
   * @private
   */
  onCopiesChanged_: function() {
    this.set(
        'model.copies',
        this.copiesValid_ ? parseInt(this.copiesString_, 10) : 1);
    this.set('model.printTicketInvalid', !this.copiesValid_);
  },

  /**
   * @return {boolean} Whether collate checkbox should be hidden.
   * @private
   */
  collateHidden_: function() {
    return !this.copiesValid_ || this.copiesString_ == '1';
  },

  /**
   * @return {boolean} Whether error message should be hidden.
   * @private
   */
  hintHidden_: function() {
    return this.copiesValid_ || this.copiesString_ == '';
  },
});
