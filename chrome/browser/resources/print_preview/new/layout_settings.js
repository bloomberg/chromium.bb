// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-layout-settings',

  behaviors: [SettingsBehavior, print_preview.SelectBehavior],

  properties: {
    disabled: Boolean,
  },

  observers: ['onLayoutSettingChange_(settings.layout.value)'],

  /**
   * @param {*} newValue The new value of the layout setting.
   * @private
   */
  onLayoutSettingChange_: function(newValue) {
    this.selectedValue =
        /** @type {boolean} */ (newValue) ? 'landscape' : 'portrait';
  },

  /** @param {string} value The new select value. */
  onProcessSelectChange: function(value) {
    this.setSetting('layout', value == 'landscape');
  },
});
