// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('device-list', {
  publish: {
    /**
     * The label of the list to be displayed.
     * @type {string}
     */
    label: 'Device List',

    /**
     * Info of the devices contained in the list.
     * @type {Array.<DeviceInfo>}
     */
    devices: null
  },

  /**
   * Called when an instance is created.
   */
  created: function() {
    this.devices = [];
  },

  /**
   * @param {string} reason The device ineligibility reason.
   * @return {string} The prettified ineligibility reason.
   * @private
   */
  prettifyReason_: function(reason) {
    if (reason == null)
      return '';
    var reasonWithSpaces = reason.replace(/([A-Z])/g, ' $1');
    return reasonWithSpaces[0].toUpperCase() + reasonWithSpaces.slice(1);
  },

  /**
   * @param {string} connectionStatus The Bluetooth connection status.
   * @return {string} The icon id to be shown for the connection state.
   * @private
   */
  getIconForConnection_: function(connectionStatus) {
    switch (connectionStatus) {
      case 'connected':
        return 'device:bluetooth-connected';
      case 'disconnected':
        return 'device:bluetooth';
      case 'connecting':
        return 'device:bluetooth-searching';
      default:
        return 'device:bluetooth-disabled';
    }
  },

  /**
   * @param {string} reason The device ineligibility reason.
   * @return {string} The icon id to be shown for the ineligibility reason.
   * @private
   */
  getIconForIneligibilityReason_: function(reason) {
    switch (reason) {
      case 'badOsVersion':
        return 'notification:system-update';
      case 'bluetoothNotSupported':
        return 'device:bluetooth-disabled';
      case 'deviceOffline':
        return 'device:signal-cellular-off';
      case 'invalidCredentials':
        return 'notification:sync-problem';
      default:
        return 'error';
    };
  }
});
