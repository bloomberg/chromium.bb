// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for network configuration selection menus.
 */
Polymer({
  is: 'network-config-select',

  behaviors: [I18nBehavior],

  properties: {
    label: String,

    disabled: Boolean,

    /**
     * Array of item values to select from.
     * @type {!Array<string>}
     */
    items: Array,

    /** Prefix used to look up ONC property names. */
    oncPrefix: String,

    /** Select item value */
    value: {
      type: String,
      notify: true,
    },
  },

  observers: ['updateSelected_(items, value)'],

  /**
   * Ensure that the <select> value is updated when |items| or |value| changes.
   * @private
   */
  updateSelected_: function() {
    // Wait for the dom-repeat to populate the <option> entries.
    this.async(function() {
      var select = this.$$('select');
      if (select.value != this.value)
        select.value = this.value;
    });
  },

  /**
   * @param {string} key
   * @param {string} prefix
   * @return {string} The text to display for the onc value.
   * @private
   */
  getOncLabel_: function(key, prefix) {
    var oncKey = 'Onc' + prefix.replace(/\./g, '-') + '_' + key;
    if (this.i18nExists(oncKey))
      return this.i18n(oncKey);
    assertNotReached();
    return key;
  },
});
