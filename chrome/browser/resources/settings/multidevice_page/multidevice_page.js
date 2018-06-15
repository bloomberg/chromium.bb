// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings page for managing MultiDevice features.
 */

/**
 * The possible statuses of hosts on the logged in account that determine the
 * page content.
 * @enum {number}
 */
settings.MultiDeviceSettingsMode = {
  NO_ELIGIBLE_HOSTS: 0,
  NO_HOST_SET: 1,
  HOST_SET_WAITING_FOR_SERVER: 2,
  HOST_SET_WAITING_FOR_VERIFICATION: 3,
  HOST_SET_VERIFIED: 4,
};

Polymer({
  is: 'settings-multidevice-page',

  behaviors: [I18nBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @type {settings.MultiDeviceSettingsMode} */
    mode: {
      type: Number,
      value: settings.MultiDeviceSettingsMode.NO_HOST_SET,
    },
  },

  /**
   * @return {string} Translated item label.
   * @private
   */
  getLabelText_: function() {
    if (this.mode == settings.MultiDeviceSettingsMode.NO_HOST_SET)
      return this.i18n('multideviceSetupItemHeading');
    // TODO(jordynass): Fill in with real device name.
    return 'Device Name Placeholder';
  },

  /**
   * @return {string} Translated sublabel.
   * @private
   */
  getSubLabelText_: function() {
    switch (this.mode) {
      case settings.MultiDeviceSettingsMode.NO_HOST_SET:
        return this.i18n('multideviceSetupSummary');
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER:
        // TODO(jordynass): Replace this with the real "Retry" string
        return 'Retry Placeholder';
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION:
        // TODO(jordynass): Replace this with the real "Verify" string
        return 'Verification Placeholder';
      default:
        // TODO(jordynass): Replace this with the i18n versions
        return 'Enabled/Disabled';
    }
  },

  /**
   * @return {string} Translated button text.
   * @private
   */
  getButtonText_: function() {
    switch (this.mode) {
      case settings.MultiDeviceSettingsMode.NO_HOST_SET:
        return this.i18n('multideviceSetupButton');
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER:
        // TODO(jordynass): Replace this with the real "Retry" string
        return 'Retry Placeholder';
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION:
        // TODO(jordynass): Replace this with the real "Verify" string
        return 'Verification Placeholder';
      default:
        return null;
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowLearnMoreLink_: function() {
    return this.mode != settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  },

  /**
   * @return {boolean}
   * @private
   */
  showButton_: function() {
    return this.mode != settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  },

  /**
   * @return {boolean}
   * @private
   */
  showToggle_: function() {
    return this.mode == settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  },
});
