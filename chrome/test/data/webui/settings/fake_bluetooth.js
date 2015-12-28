// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.bluetooth for testing.
 */
cr.define('settings', function() {
  /**
   * Fake of the chrome.bluetooth API.
   * @constructor
   * @implements {Bluetooth}
   */
  function FakeBluetooth() {
    /** @type {boolean} */ this.enabled = false;

    /** @type {!Array<!chrome.bluetooth.Device>} */ this.devices = [];
  }

  FakeBluetooth.prototype = {
    // Public testing methods.
    /**
     * @param {!Array<!chrome.bluetooth.Device>} devices
     */
    setDevicesForTest: function(devices) {
      for (var d of this.devices)
        this.onDeviceRemoved.callListeners(d);
      this.devices = devices;
      for (var d of this.devices)
        this.onDeviceAdded.callListeners(d);
    },

    // Bluetooth overrides.
    /** @override */
    getAdapterState: function(callback) {
      setTimeout(function() {
        callback({
          address: '00:11:22:33:44:55:66',
          name: 'Fake Adapter',
          powered: this.enabled,
          available: true,
          discovering: false
        });
      }.bind(this));
    },

    /** @override */
    getDevice: assertNotReached,

    /** @override */
    getDevices: function(callback) {
      setTimeout(function() {
        callback(this.devices);
      }.bind(this));
    },

    /** @override */
    startDiscovery: assertNotReached,

    /** @override */
    stopDiscovery: assertNotReached,

    /** @override */
    onAdapterStateChanged: new FakeChromeEvent(),

    /** @override */
    onDeviceAdded: new FakeChromeEvent(),

    /** @override */
    onDeviceChanged: new FakeChromeEvent(),

    /** @override */
    onDeviceRemoved: new FakeChromeEvent(),
  };

  return {FakeBluetooth: FakeBluetooth};
});
