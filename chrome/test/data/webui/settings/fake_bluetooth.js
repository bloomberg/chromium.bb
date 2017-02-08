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
    /** @type {!chrome.bluetooth.AdapterState} */ this.adapterState = {
      address: '00:11:22:33:44:55:66',
      name: 'Fake Adapter',
      powered: false,
      available: true,
      discovering: false
    };

    /** @type {!Array<!chrome.bluetooth.Device>} */ this.devices = [];
  }

  FakeBluetooth.prototype = {
    // Public testing methods.
    /** @param {boolean} enabled */
    setEnabled: function(enabled) {
      this.adapterState.powered = enabled;
      this.onAdapterStateChanged.callListeners(this.adapterState);
    },

    /** @param {!Array<!chrome.bluetooth.Device>} devices */
    setDevicesForTest: function(devices) {
      for (var d of this.devices)
        this.onDeviceRemoved.callListeners(d);
      this.devices = devices;
      for (var d of this.devices)
        this.onDeviceAdded.callListeners(d);
    },

    /**
     * @param {string}
     * @return {!chrome.bluetooth.Device}
     */
    getDeviceForTest: function(address) {
      return this.devices.find(function(d) {
        return d.address == address;
      });
    },

    /** @param {!chrome.bluetooth.Device} device */
    updateDeviceForTest: function(device, opt_callback) {
      var index = this.devices.findIndex(function(d) {
        return d.address == device.address;
      });
      if (index == -1) {
        this.devices.push(device);
        this.onDeviceAdded.callListeners(device);
        return;
      }
      this.devices[index] = device;
      this.onDeviceChanged.callListeners(device);
    },

    // Bluetooth overrides.
    /** @override */
    getAdapterState: function(callback) {
      callback(this.adapterState);
    },

    /** @override */
    getDevice: assertNotReached,

    /** @override */
    getDevices: function(callback) {
      callback(this.devices);
    },

    /** @override */
    startDiscovery: function(callback) {
      callback();
    },

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
