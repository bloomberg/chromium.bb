// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'content-panel',

  properties: {
    /**
     * List of devices that are online and reachable by CryptAuth.
     * @type {Array<DeviceInfo>}
     * @const
     * @private
     */
    onlineDevices_: {
      type: Array,
      value: [
        {
          publicKey: 'CAESRgohAN4IwMl-bTemMQ0E-YV36s86TG0suEj422xKvNoIslyvEiE' +
                     'Ab8m8J4dB44v2l8l-38-9vLwzf9GsPth4BsquCpAbNRk=',
          friendlyDeviceName: 'Sony Xperia Z3',
        },
        {
          publicKey: 'CAESRgohAN5ljWzNNYWbuQNoQdE0NhZaCg9CmCVd-XuwR__41G_5EiE' +
                     'A3ATFvesN_6Tr-wSM5fHL5v5Bn1mX4d-m4OXJ3beRYhk=',
          friendlyDeviceName: 'Samsung Galaxy S5',
        },
        {
          publicKey: 'CAESRgohAN9QYU5HySO14Gi9PDIClacBnC0C8wqPwXsNHUNG_vXlEiE' +
                     'AggzU80ZOd9DWuCBdp6bzpGcC-oj1yrwdVCHGg_yeaAQ=',
          friendlyDeviceName: 'LGE Nexus 5',
        },
        {
          publicKey: 'CAESRgohAOGtyjLiPnkQwi-bAvp75hUrBkfRlx2pBw7_C0VojjIFEiE' +
                     'ARMN6miPfDOtlAFOiV7fulvhoDqguq-sTjUCZ2Et0mYQ=',
          friendlyDeviceName: 'LGE Nexus 4',
          unlockKey: true,
          connectionStatus: 'disconnected',
          ineligibilityReasons: [
            'deviceOffline',
            'invalidCredentials',
            'bluetoothNotSupported',
            'badOsVersion'
          ],
          remoteState: {
            userPresent: true,
            secureScreenLock: false,
            trustAgent: true,
          }
        },
        {
          publicKey: 'CAESRgohAOKowGAhRs6FryK5KZqlHoAinNCon0T1tpeyIGPzKKmlEiE' +
                     'ARH5joqbVWtLGjuh7aO7nLEhkFxv0u2C3kyoWysq6U_4=',
          friendlyDeviceName: 'Samsung GT-N8010',
        },
        {
          publicKey: 'CAESRgohAOPegrIJNl9HeFeykbCswXciAXOpJZdme6Nh2WcMMiyZEiE' +
                     'A8X7fTDRh61X-iDE1bYASMPiCbk7bPy7n0N0MiBeJNuo=',
          friendlyDeviceName: 'Sony D5503',
        },
        {
          publicKey: 'CAESRgohAOZly-M7bBQokc3CJ9Wio37bzpUYaD-p9On3e6H4n2kKEiE' +
                     'ANhkioq9y_19lN8Wnoc8XBLOilyyT6kn6iM00DEIOhFk=',
          friendlyDeviceName: 'LGE LG-D855',
        },
        {
          publicKey: 'CAESRgohAOZ6JXDntf-h7MmRgrJc9RuW0mIgDluYEBcs8zx_uESeEiE' +
                     'AeQk_My_0AFM9jb0eOSZupZ2s_n-6Vqs-_XaOnbAGSeU=',
          friendlyDeviceName: 'Motorola Nexus 6',
        },
      ],
      readOnly: true,
    },

    /**
     * The index of the selected page that is currently shown.
     * @private
     */
    selected_: {
      type: Number,
      value: 0,
    }
  },

  /**
   * Called when a page transition event occurs.
   * @param {Event} event
   * @private
   */
  onSelectedPageChanged_: function(event) {
    var newPage = event.detail.value instanceof Element &&
                  event.detail.value.children[0];
    if (newPage && newPage.activate != null) {
      newPage.activate();
    }
  }
});
