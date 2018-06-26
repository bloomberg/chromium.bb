// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Container element that interfaces with the
 * MultiDeviceBrowserProxy to ensure that if there is not a host device or
 * potential host device
 *     (a) the entire multidevice settings-section is hidden and
 *     (b) the settings-multidevice-page is not attched to the DOM.
 * It also provides the settings-multidevice-page with the data it needs to
 * provide the user with the correct infomation and call(s) to action based on
 * the data retrieved from the browser proxy (i.e. the mode_ property).
 */

Polymer({
  is: 'settings-multidevice-page-container',

  properties: {
    /** SettingsPrefsElement 'prefs' Object reference. See prefs.js. */
    prefs: {
      type: Object,
      notify: true,
    },

    // TODO(jordynass): Set this based on data retrieved by browser proxy.
    /** @type {settings.MultiDeviceSettingsMode} */
    mode_: Number,

    /**
     * Whether a phone was found on the account that is either connected to the
     * Chromebook or has the potential to be.
     * @type {boolean}
     */
    doesPotentialConnectedPhoneExist: {
      notify: true,
      type: Boolean,
      computed: 'computeDoesPotentialConnectedPhoneExist(mode_)',
    },
  },

  /** @override */
  ready: function() {
    this.mode_ = settings.MultiDeviceSettingsMode.NO_HOST_SET;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeDoesPotentialConnectedPhoneExist: function() {
    return this.mode_ != settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS;
  },
});
