// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-bluetooth-page' is the settings page for managing bluetooth
 *  properties and devices.
 *
 * Example:
 *    <core-animated-pages>
 *      <settings-bluetooth-page>
 *      </settings-bluetooth-page>
 *      ... other pages ...
 *    </core-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-bluetooth-page
 */
Polymer({
  is: 'settings-bluetooth-page',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /** The current active route. */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /** Whether bluetooth is enabled. */
    bluetoothEnabled: {type: Boolean, value: false},

    /**
     * The ordered list of bluetooth devices.
     * @type {!Array<!chrome.bluetooth.Device>}
     */
    deviceList: {type: Array, value: function() { return []; }},

    /** The index of the selected device or -1 if none. */
    selectedDevice: {type: Number, value: -1}
  },

  /**
   * Listener for chrome.bluetooth.onAdapterStateChanged events.
   * @type {function(!chrome.bluetooth.AdapterState)|undefined}
   * @private
   */
  bluetoothAdapterStateChangedListener_: undefined,

  /**
   * Listener for chrome.bluetooth.onBluetoothDeviceAdded/Changed events.
   * @type {function(!chrome.bluetooth.Device)|undefined}
   * @private
   */
  bluetoothDeviceUpdatedListener_: undefined,

  /**
   * Listener for chrome.bluetooth.onBluetoothDeviceRemoved events.
   * @type {function(!chrome.bluetooth.Device)|undefined}
   * @private
   */
  bluetoothDeviceRemovedListener_: undefined,


  /** @override */
  attached: function() {
    this.bluetoothAdapterStateChangedListener_ =
        this.onBluetoothAdapterStateChanged_.bind(this);
    chrome.bluetooth.onAdapterStateChanged.addListener(
        this.bluetoothAdapterStateChangedListener_);

    this.bluetoothDeviceUpdatedListener_ =
        this.onBluetoothDeviceUpdated_.bind(this);
    chrome.bluetooth.onDeviceAdded.addListener(
        this.bluetoothDeviceUpdatedListener_);
    chrome.bluetooth.onDeviceChanged.addListener(
        this.bluetoothDeviceUpdatedListener_);

    this.bluetoothDeviceRemovedListener_ =
        this.onBluetoothDeviceRemoved_.bind(this);
    chrome.bluetooth.onDeviceRemoved.addListener(
        this.bluetoothDeviceRemovedListener_);

    // Request the inital adapter state.
    chrome.bluetooth.getAdapterState(
        this.bluetoothAdapterStateChangedListener_);
  },

  /** @override */
  detached: function() {
    if (this.bluetoothAdapterStateChangedListener_) {
      chrome.bluetooth.onAdapterStateChanged.removeListener(
          this.bluetoothAdapterStateChangedListener_);
    }
    if (this.bluetoothDeviceUpdatedListener_) {
      chrome.bluetooth.onDeviceAdded.removeListener(
          this.bluetoothDeviceUpdatedListener_);
      chrome.bluetooth.onDeviceChanged.removeListener(
          this.bluetoothDeviceUpdatedListener_);
    }
    if (this.bluetoothDeviceRemovedListener_) {
      chrome.bluetooth.onDeviceRemoved.removeListener(
          this.bluetoothDeviceRemovedListener_);
    }
  },

  /**
   * If bluetooth is enabled, request the complete list of devices and update
   * |deviceList|.
   * @private
   */
  updateDeviceList_: function() {
    if (!this.bluetoothEnabled) {
      this.deviceList = [];
      return;
    }
    chrome.bluetooth.getDevices(function(devices) {
      this.deviceList = devices;
    }.bind(this));
  },

  /**
   * Event called when a user action changes the bluetoothEnabled state.
   * @private
   */
  onBluetoothEnabledChange_: function() {
    chrome.bluetoothPrivate.setAdapterState(
        {powered: this.bluetoothEnabled}, function() {
          if (chrome.runtime.lastError) {
            console.error(
                'Error enabling bluetooth: ' +
                chrome.runtime.lastError.message);
          }
        });
  },

  /**
   * Process bluetooth.onAdapterStateChanged events.
   * @param {!chrome.bluetooth.AdapterState} state
   * @private
   */
  onBluetoothAdapterStateChanged_: function(state) {
    this.bluetoothEnabled = state.powered;
    this.updateDeviceList_();
  },

  /**
   * Process bluetooth.onDeviceAdded and onDeviceChanged events.
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  onBluetoothDeviceUpdated_: function(device) {
    if (!device)
      return;
    var address = device.address;
    var index = this.getDeviceIndex_(address);
    if (index >= 0) {
      this.set('deviceList.' + index, device);
      return;
    }
    this.push('deviceList', device);
  },

  /**
   * Process bluetooth.onDeviceRemoved events.
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  onBluetoothDeviceRemoved_: function(device) {
    var address = device.address;
    var index = this.getDeviceIndex_(address);
    if (index < 0)
      return;
    this.splice('deviceList', index, 1);
  },

  /**
   * @param {string} address
   * @return {number} The index of the device associated with |address| or -1.
   * @private
   */
  getDeviceIndex_: function(address) {
    var len = this.deviceList.length;
    for (var i = 0; i < len; ++i) {
      if (this.deviceList[i].address == address)
        return i;
    }
    return -1;
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @return {string} The text to display for |device| in the device list.
   * @private
   */
  getDeviceText_: function(device) {
    if (device.connecting)
      return this.i18n('bluetoothConnecting', device.name);
    return device.name || device.address;
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @return {boolean} True if |device| should be shown in the device list.
   * @private
   */
  showDeviceInList_: function(device) {
    return !!device.paired || !!device.connected || !!device.connecting;
  },

  /**
   * @param {!Array<!chrome.bluetooth.Device>} deviceList
   * @return {boolean} True if deviceList is not empty.
   * @private
   */
  haveDevices_: function(deviceList) { return !!deviceList.length; },

  /**
   * @param {number} selectedDevice
   * @return {boolean} True if a device is selected.
   * @private
   */
  haveSelectedDevice_: function(selectedDevice) { return selectedDevice >= 0; },

  /** @private */
  onConnectTap_: function() {
    if (this.selectedDevice < 0)
      return;
    var device = this.deviceList[this.selectedDevice];
    if (!device)
      return;
    chrome.bluetoothPrivate.connect(device.address, function(result) {
      if (chrome.runtime.lastError) {
        console.error(
            'Error connecting to: ' + device.address +
            chrome.runtime.lastError.message);
      }
    });
  },

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveTap_: function(event) {
    var address = event.target.address;
    var index = this.getDeviceIndex_(address);
    if (index < 0)
      return;
    var device = this.deviceList[index];
    if (device.connected) {
      chrome.bluetoothPrivate.disconnectAll(address, function() {
        if (chrome.runtime.lastError) {
          console.error(
              'Error disconnecting devce: ' + device.name + ': ' +
              chrome.runtime.lastError.message);
        }
      });
    } else {
      chrome.bluetoothPrivate.forgetDevice(address, function() {
        if (chrome.runtime.lastError) {
          console.error(
              'Error forgetting devce: ' + device.name + ': ' +
                  chrome.runtime.lastError.message);
        }
      });
    }
  },

  /** @private */
  onAddDeviceTap_: function() {}
});
