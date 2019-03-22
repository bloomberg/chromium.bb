// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview_new');

/**
 * |key| is the field in the serialized settings state that corresponds to the
 * setting, or an empty string if the setting should not be saved in the
 * serialized state.
 * @typedef {{
 *   value: *,
 *   unavailableValue: *,
 *   valid: boolean,
 *   available: boolean,
 *   setByPolicy: boolean,
 *   key: string,
 *   updatesPreview: boolean,
 * }}
 */
print_preview_new.Setting;

/**
 * @typedef {{
 *   pages: !print_preview_new.Setting,
 *   copies: !print_preview_new.Setting,
 *   collate: !print_preview_new.Setting,
 *   layout: !print_preview_new.Setting,
 *   color: !print_preview_new.Setting,
 *   mediaSize: !print_preview_new.Setting,
 *   margins: !print_preview_new.Setting,
 *   dpi: !print_preview_new.Setting,
 *   fitToPage: !print_preview_new.Setting,
 *   scaling: !print_preview_new.Setting,
 *   duplex: !print_preview_new.Setting,
 *   duplexShortEdge: !print_preview_new.Setting,
 *   cssBackground: !print_preview_new.Setting,
 *   selectionOnly: !print_preview_new.Setting,
 *   headerFooter: !print_preview_new.Setting,
 *   rasterize: !print_preview_new.Setting,
 *   vendorItems: !print_preview_new.Setting,
 *   otherOptions: !print_preview_new.Setting,
 *   ranges: !print_preview_new.Setting,
 *   pagesPerSheet: !print_preview_new.Setting,
 *   pin: (print_preview_new.Setting|undefined),
 *   pinValue: (print_preview_new.Setting|undefined),
 * }}
 */
print_preview_new.Settings;

/** @polymerBehavior */
const SettingsBehavior = {
  properties: {
    /** @type {print_preview_new.Settings} */
    settings: Object,
  },

  /**
   * @param {string} settingName Name of the setting to get.
   * @return {print_preview_new.Setting} The setting object.
   */
  getSetting: function(settingName) {
    return print_preview.Model.getInstance().getSetting(settingName);
  },

  /**
   * @param {string} settingName Name of the setting to get the value for.
   * @return {*} The value of the setting, accounting for availability.
   */
  getSettingValue: function(settingName) {
    return print_preview.Model.getInstance().getSettingValue(settingName);
  },

  /**
   * Sets settings.settingName.value to |value|, unless updating the setting is
   * disallowed by enterprise policy. Fires preview-setting-changed and
   * sticky-setting-changed events if the update impacts the preview or requires
   * an update to sticky settings.
   * @param {string} settingName Name of the setting to set
   * @param {*} value The value to set the setting to.
   */
  setSetting: function(settingName, value) {
    print_preview.Model.getInstance().setSetting(settingName, value);
  },

  /**
   * @param {string} settingName Name of the setting to set
   * @param {number} start
   * @param {number} end
   * @param {*} newValue The value to add (if any).
   */
  setSettingSplice: function(settingName, start, end, newValue) {
    print_preview.Model.getInstance().setSettingSplice(
        settingName, start, end, newValue);
  },

  /**
   * Sets the validity of |settingName| to |valid|. If the validity is changed,
   * fires a setting-valid-changed event.
   * @param {string} settingName Name of the setting to set
   * @param {boolean} valid Whether the setting value is currently valid.
   */
  setSettingValid: function(settingName, valid) {
    print_preview.Model.getInstance().setSettingValid(settingName, valid);
  },
};
