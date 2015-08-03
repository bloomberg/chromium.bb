// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A bluetooth device.
 * @constructor
 */
var BluetoothDevice = function() {
  this.address = '';
  this.alias = '';

  // The text label of the selected device class.
  this.class = 'Computer';

  // The uint32 value of the selected device class.
  this.classValue = 0x104;

  this.isTrusted = true;
  this.name = '';
  this.path = '';

  // The label of the selected pairing method option.
  this.pairingMethod = 'None';

  // The text containing a PIN key or passkey for pairing.
  this.pairingAuthToken = '';
};

Polymer({
  is: 'bluetooth-settings',

  properties: {
    /**
     * The title to be displayed in a heading element for the element.
     */
    title: {
      type: String
    },

    /**
     * A set of bluetooth devices.
     * @type !Array<!BluetoothDevice>
     */
    devices: {
      type: Array,
      value: function() { return []; }
    },

    /**
     * A set of options for the possible bluetooth device classes/types.
     * Object |value| attribute comes from values in the WebUI, set in
     * setDeviceClassOptions.
     * @type !Array<! {text: string, value: int} >
     */
    deviceClassOptions: {
      type: Array,
      value: function() {
        return [{ text: 'Unknown', value: 0 },
                { text: 'Mouse', value: 0x2580 },
                { text: 'Keyboard', value: 0x2540 },
                { text: 'Audio', value: 0x240408 },
                { text: 'Phone', value: 0x7a020c },
                { text: 'Computer', value: 0x104 }];
      }
    },

    /**
     * A set of strings representing the method to be used for
     * authenticating a device during a pair request.
     * @type !Array<string>
     */
    deviceAuthenticationMethods: {
      type: Array,
      value: function() { return ['None', 'PIN Code', 'PassKey']; }
    },

    /**
     * Contains keys for all the device paths which have been discovered. Used
     * to look up whether or not a device is listed already.
     * @type {Object}
     */
    devicePaths: {
      type: Object,
      value: function() { return {}; }
    },
  },

  ready: function() {
    this.title = 'Bluetooth Settings';
  },

  /**
   * Checks whether or not the PIN/passkey input field should be shown.
   * It should only be shown when the pair method is not 'None' or empty.
   * @param {string} pairMethod The label of the selected pair method option
   *     for a particular device.
   * @return {boolean} Whether the PIN/passkey input field should be shown.
   */
  showAuthToken: function(pairMethod) {
    return pairMethod && pairMethod != 'None';
  },

  /**
   * Called by the WebUI which provides a list of devices which are connected
   * to the main adapter.
   * @param {!Array<!BluetoothDevice>} devices A list of bluetooth devices.
   */
  updateBluetoothInfo: function(devices) {
    /** @type {!Array<!BluetoothDevice>} */ var deviceList = [];

    for (var i = 0; i < devices.length; ++i) {
      // Get the label for the device class which should be selected.
      devices[i].class = this.getTextForDeviceClass(devices[i]['classValue']);
      deviceList.push(devices[i]);
      this.devicePaths[devices[i]['path']] = true;
    }

    this.devices = deviceList;
  },

  pairDevice: function(e) {
    var device = this.devices[e.path[2].dataIndex];
    device.classValue = this.getValueForDeviceClass(device.class);
    this.devicePaths[device.path] = true;

    // Send device info to the WebUI.
    chrome.send('requestBluetoothPair', [device]);
  },

  discoverDevice: function(e) {
    var device = this.devices[e.path[2].dataIndex];
    device.classValue = this.getValueForDeviceClass(device.class);
    this.devicePaths[device.path] = true;

    // Send device info to WebUI.
    chrome.send('requestBluetoothDiscover', [device]);
  },

  // Adds a new device with default settings to the list of devices.
  appendNewDevice: function() {
    this.push('devices', new BluetoothDevice());
  },

  /**
   * This is called when a new device is discovered by the main adapter.
   * The device is only added to the view's list if it is not already in
   * the list (i.e. its path has not yet been recorded in |devicePaths|).
   * @param {BluetoothDevice} device A bluetooth device.
   */
  addBluetoothDevice: function(device) {
    if (this.devicePaths[device['path']] != undefined)
      return;

    device.class = this.getTextForDeviceClass(device['classValue']);
    this.push('devices', device);
    this.devicePaths[device['path']] = true;
  },

  /**
   * Removes the bluetooth device with path |path|.
   * @param {string} path A bluetooth device's path.
   */
  removeBluetoothDevice: function(path) {
    if (this.devicePaths[path] == undefined)
      return;

    for (var i = 0; i < this.devices.length; ++i) {
      if (this.devices[i].path == path) {
        this.splice('devices', i, 1);
        break;
      }
    }
  },

  /**
   * Returns the text for the label that corresponds to |classValue|.
   * @param {number} classValue A number representing the bluetooth class
   *     of a device.
   * @return {string} The label which represents |classValue|.
   */
  getTextForDeviceClass: function(classValue) {
    for (var i = 0; i < this.deviceClassOptions.length; ++i) {
      if (this.deviceClassOptions[i].value == classValue)
        return this.deviceClassOptions[i].text;
    }
  },

  /**
   * Returns the integer value which corresponds with the label |classText|.
   * @param {string} classText The label for a device class option.
   * @return {number} The value which |classText| represents.
   */
  getValueForDeviceClass: function(classText) {
    for (var i = 0; i < this.deviceClassOptions.length; ++i) {
      if (this.deviceClassOptions[i].text == classText)
        return this.deviceClassOptions[i].value;
    }
    return 0;
  },
});
