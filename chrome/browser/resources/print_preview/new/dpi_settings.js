// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-dpi-settings',

  properties: {
    /** @type {{ option: Array<!print_preview_new.SelectOption> }} */
    capability: Object,

    /** @private {{ option: Array<!print_preview_new.SelectOption> }} */
    capabilityWithLabels_: {
      type: Object,
      computed: 'computeCapabilityWithLabels_(capability)',
    },
  },

  /**
   * Adds default labels for each option.
   * @return {{option: Array<!print_preview_new.SelectOption>}}
   * @private
   */
  computeCapabilityWithLabels_: function() {
    if (!this.capability || !this.capability.option)
      return this.capability;
    const result =
        /** @type {{option: Array<!print_preview_new.SelectOption>}} */ (
            JSON.parse(JSON.stringify(this.capability)));
    this.capability.option.forEach((option, index) => {
      const hDpi = option.horizontal_dpi || 0;
      const vDpi = option.vertical_dpi || 0;
      if (hDpi > 0 && vDpi > 0 && hDpi != vDpi) {
        result.option[index].name = loadTimeData.getStringF(
            'nonIsotropicDpiItemLabel', hDpi.toLocaleString(),
            vDpi.toLocaleString());
      } else {
        result.option[index].name = loadTimeData.getStringF(
            'dpiItemLabel', (hDpi || vDpi).toLocaleString());
      }
    });
    return result;
  }
});
