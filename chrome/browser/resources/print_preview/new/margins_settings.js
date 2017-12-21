// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-margins-settings',

  behaviors: [SettingsBehavior],

  observers: ['onMarginsSettingChange_(settings.margins.value)'],

  /**
   * @param {*} value The new value of the margins setting.
   * @private
   */
  onMarginsSettingChange_: function(value) {
    this.$$('select').value = /** @type {string} */ (value).toString();
  },

  /** @private */
  onChange_: function() {
    this.setSetting('margins', parseInt(this.$$('select').value, 10));
  },
});
