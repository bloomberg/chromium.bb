// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @polymer */
Polymer({
  is: 'multidevice-setup',

  properties: {
    /**
     * Array of objects representing all available MultiDevice hosts. Each
     * object contains the name of the device type (e.g. "Pixel XL") and its
     * Device ID.
     *
     * @private
     * @type {Array<{name: string, id: string}>}
     */
    devices_: Array,

    /**
     * Element name of the currently visible page.
     *
     * @private
     * @type {string}
     */
    visiblePageName_: {
      type: String,
      value: 'start-setup-page',
    },

    /**
     * DOM Element corresponding to the visible page.
     *
     * @private
     * @type {!StartSetupPageElement|!SettingUpPageElement|
     *        !SetupSucceededPageElement|!SetupFailedPageElement}
     */
    visiblePage_: Object,

    /**
     *  Device ID for the currently selected host device.
     *
     *  Undefined if the no list of potential hosts has been received from
     *  CryptAuth.
     *
     *  @private
     *  @type {string|undefined}
     */
    selectedDeviceId_: String,
  },

  listeners: {
    'forward-navigation-requested': 'onForwardNavigationRequested_',
    'backward-navigation-requested': 'onBackwardNavigationRequested_',
  },

  // Event handling callbacks

  /** @private */
  onForwardNavigationRequested_: function() {
    // TODO(jordynass): Put in page specific logic as each page is implemented.
  },

  /** @private */
  onBackwardNavigationRequested_: function() {
    // TODO(jordynass): Put in page specific logic as each page is implemented.
  },
});
