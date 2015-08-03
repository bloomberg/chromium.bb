// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'device-list',

  properties: {
    /**
     * The label of the list to be displayed.
     * @type {string}
     */
    label: {
      type: String,
      value: 'Device List',
    },

    /**
     * Info of the devices contained in the list.
     * @type {Array<DeviceInfo>}
     */
    devices: Array,
  },

  /**
   * Handles when the toggle connection button is clicked for a list item.
   * @param {Event} event
   */
  toggleConnection_: function(event) {
    var deviceInfo = event.model.item;
    chrome.send('toggleConnection', [deviceInfo.publicKey]);
  },

  /**
   * @param {string} reason The device ineligibility reason.
   * @return {string} The prettified ineligibility reason.
   * @private
   */
  prettifyReason_: function(reason) {
    if (reason == null || reason == '')
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
   * @param {DeviceInfo} device
   * @return {string} The icon id to be shown for the unlock key state of the
   *     device.
   */
  getIconForUnlockKey_: function(device) {
    return 'hardware:phonelink' + (!device.unlockKey ? '-off' : '');
  },

  /**
   * @param {Object} remoteState The remote state of the device.
   * @return {string} The icon representing the state.
   */
  getIconForRemoteState_: function(remoteState) {
    if (remoteState != null && remoteState.userPresent &&
        remoteState.secureScreenLock && remoteState.trustAgent) {
      return 'icons:lock-open';
    } else {
      return 'icons:lock-outline';
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
  },

  /**
   * @param {number} userPresence
   * @return {string}
   */
  getUserPresenceText_: function(userPresence) {
    var userPresenceMap = {
      0: 'User Present',
      1: 'User Absent',
      2: 'User Presence Unknown',
    };
    return userPresenceMap[userPresence];
  },

  /**
   * @param {number} screenLock
   * @return {string}
   */
  getScreenLockText_: function(screenLock) {
    var screenLockMap = {
      0: 'Secure Screen Lock Enabled',
      1: 'Secure Screen Lock Disabled',
      2: 'Secure Screen Lock State Unknown',
    };
    return screenLockMap[screenLock];
  },

  /**
   * @param {number} trustAgent
   * @return {string}
   */
  getTrustAgentText_: function(trustAgent) {
    var trustAgentMap = {
      0: 'Trust Agent Enabled',
      1: 'Trust Agent Disabled',
      2: 'Trust Agent Unsupported',
    };
    return trustAgentMap[trustAgent];
  },
});
