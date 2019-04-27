// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/**
 * @typedef {{
 *   horizontal_dpi: (number | undefined),
 *   vertical_dpi: (number | undefined),
 *   vendor_id: (number | undefined)}}
 */
print_preview.DpiOption;

/**
 * @typedef {{
 *   horizontal_dpi: (number | undefined),
 *   name: string,
 *   vertical_dpi: (number | undefined),
 *   vendor_id: (number | undefined)}}
 */
print_preview.LabelledDpiOption;

Polymer({
  is: 'print-preview-dpi-settings',

  behaviors: [SettingsBehavior],

  properties: {
    /** @type {{ option: Array<!print_preview.SelectOption> }} */
    capability: Object,

    disabled: Boolean,

    /** @private {{ option: Array<!print_preview.SelectOption> }} */
    capabilityWithLabels_: {
      type: Object,
      computed: 'computeCapabilityWithLabels_(capability)',
    },
  },

  observers: [
    'onDpiSettingChange_(settings.dpi.*, capabilityWithLabels_.option)',
  ],

  /**
   * Adds default labels for each option.
   * @return {?{option: Array<!print_preview.SelectOption>}}
   * @private
   */
  computeCapabilityWithLabels_: function() {
    if (this.capability === undefined) {
      return null;
    }

    const result =
        /** @type {{option: Array<!print_preview.SelectOption>}} */ (
            JSON.parse(JSON.stringify(this.capability)));
    this.capability.option.forEach((option, index) => {
      const dpiOption = /** @type {print_preview.DpiOption} */ (option);
      const hDpi = dpiOption.horizontal_dpi || 0;
      const vDpi = dpiOption.vertical_dpi || 0;
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
  },

  /** @private */
  onDpiSettingChange_: function() {
    if (this.capabilityWithLabels_ === null ||
        this.capabilityWithLabels_ === undefined) {
      return;
    }

    const dpiValue =
        /** @type {print_preview.DpiOption} */ (this.getSettingValue('dpi'));
    for (const option of assert(this.capabilityWithLabels_.option)) {
      const dpiOption =
          /** @type {print_preview.LabelledDpiOption} */ (option);
      if (dpiValue.horizontal_dpi == dpiOption.horizontal_dpi &&
          dpiValue.vertical_dpi == dpiOption.vertical_dpi &&
          dpiValue.vendor_id == dpiOption.vendor_id) {
        this.$$('print-preview-settings-select')
            .selectValue(JSON.stringify(option));
        return;
      }
    }

    const defaultOption =
        this.capabilityWithLabels_.option.find(o => !!o.is_default) ||
        this.capabilityWithLabels_.option[0];
    this.setSetting('dpi', defaultOption);
  },
});
