// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   tetherNostDeviceName: string,
 *   batteryPercentage: number,
 *   connectionStrength: number,
 *   isTetherHostCurrentlyOnWifi: boolean
 * }}
 */
var TetherConnectionData;

Polymer({
  is: 'tether-connection-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The data used to display the tether connection dialog.
     * @private {!TetherConnectionData|undefined}
     */
    tetherData_: {
      type: Object,
      // TODO(khorimoto): Remove this and use real data when available.
      value: {
        tetherNostDeviceName: 'Pixel XL',
        batteryPercentage: 100,
        connectionStrength: 4,
        isTetherHostCurrentlyOnWifi: false
      },
    },
  },

  open: function() {
    var dialog = this.getDialog_();
    if (!dialog.open)
      this.getDialog_().showModal();
  },

  close: function() {
    var dialog = this.getDialog_();
    if (dialog.open)
      dialog.close();
  },

  /**
   * @return {!CrDialogElement}
   * @private
   */
  getDialog_: function() {
    return /** @type {!CrDialogElement} */ (this.$.dialog);
  },

  /** @private */
  onNotNowTap_: function() {
    this.getDialog_().cancel();
  },

  /**
   * @param {!TetherConnectionData|undefined} tetherData
   * @return {string}
   * @private
   */
  getReceptionIcon_: function(tetherData) {
    var connectionStrength;

    if (!tetherData || !tetherData.connectionStrength) {
      connectionStrength = 0;
    } else {
      // Ensure that 0 <= connectionStrength <= 4, since these values are the
      // limits of the cellular strength icons.
      connectionStrength =
          Math.min(Math.max(tetherData.connectionStrength, 0), 4);
    }

    return 'settings:signal-cellular-' + connectionStrength + '-bar';
  }
});
