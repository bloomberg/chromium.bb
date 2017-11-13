// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview_new', function() {

  /** @polymerBehavior */
  const InputSettingsBehavior = {
    /** @type {string} */
    defaultValue: '0',

    properties: {
      /** @type {string} */
      inputString: {
        type: String,
        notify: true,
      },

      /** @type {boolean} */
      inputValid: {
        type: Boolean,
        notify: true,
        computed: 'computeValid_(inputString)',
      },
    },

    /** @override */
    attached: function() {
      this.set('inputString', this.defaultValue);
    },

    /**
     * @param {!KeyboardEvent} e The keyboard event
     * @private
     */
    onKeydown_: function(e) {
      if (e.key == '.' || e.key == 'e' || e.key == '-')
        e.preventDefault();
    },

    /** @private */
    onBlur_: function() {
      if (this.inputString == '')
        this.set('inputString', this.defaultValue);
    },

    /**
     * @return {boolean} Whether input value represented by inputString is
     *     valid.
     * @private
     */
    computeValid_: function() {
      // Make sure value updates first, in case inputString was updated by JS.
      this.$$('.user-value').value = this.inputString;
      return this.$$('.user-value').validity.valid && this.inputString != '';
    },

    /**
     * @return {boolean} Whether error message should be hidden.
     * @private
     */
    hintHidden_: function() {
      return this.inputValid || this.inputString == '';
    },
  };

  return {InputSettingsBehavior: InputSettingsBehavior};
});
