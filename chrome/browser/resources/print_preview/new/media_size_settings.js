// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-media-size-settings',

  behaviors: [SettingsBehavior],

  properties: {
    capability: Object,

    disabled: Boolean,
  },

  observers:
      ['onMediaSizeSettingChange_(settings.mediaSize.*, capability.option)'],

  /** @private */
  onMediaSizeSettingChange_: function() {
    if (!this.capability) {
      return;
    }
    const valueToSet = JSON.stringify(this.getSettingValue('mediaSize'));
    for (const option of
         /** @type {!Array<!print_preview.SelectOption>} */ (
             this.capability.option)) {
      if (JSON.stringify(option) === valueToSet) {
        this.$$('print-preview-settings-select').selectValue(valueToSet);
        return;
      }
    }

    const defaultOption = this.capability.option.find(o => !!o.is_default) ||
        this.capability.option[0];
    this.setSetting('mediaSize', defaultOption);
  },
});
