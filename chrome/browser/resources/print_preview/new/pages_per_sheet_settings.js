// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-pages-per-sheet-settings',

  behaviors: [SettingsBehavior],

  properties: {
    disabled: Boolean,
  },

  observers: ['onPagesPerSheetSettingChange_(settings.pagesPerSheet.value)'],

  /**
   * @param {*} value The new value of the pages per sheet setting.
   * @private
   */
  onPagesPerSheetSettingChange_: function(value) {
    this.$$('select').value = /** @type {number} */ (value).toString();
  },

  /** @private */
  onChange_: function() {
    this.setSetting('pagesPerSheet', parseInt(this.$$('select').value, 10));
  },
});
