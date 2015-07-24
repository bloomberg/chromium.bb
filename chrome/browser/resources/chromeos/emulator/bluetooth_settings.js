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
  this.class = 'Computer';
  this.classValue = 0;
  this.isPairable = true;
  this.isTrusted = true;
  this.name = '';
  this.path = '';
  this.pairMethod = 'None';
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
     * Indicates whether or not the main bluetooth adapter is turned on.
     */
    powerToMainAdapter: {
      type: Boolean
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
     * @type !Array<! { text: string, value: int }} >
     */
    deviceClassOptions: {
      type: Array,
      value: function() {
        return [{ text: 'Unknown', value: 0 },
                { text: 'Mouse', value: 0x2580 },
                { text: 'Keyboard', value: 0x2540 },
                { text: 'Audio', value: 0x240408 },
                { text: 'Phone', value: 0x7a020c },
                { text: 'Computer', value: 0x100 }];
      }
    },

    /**
     * A set of strings representing the method to be used for
     * authenticating a device during a pair request.
     * @type !Array<!String>
     */
    deviceAuthenticationMethods: {
      type: Array,
      value: function() { return ['None', 'Pin Code', 'Pass Key']; }
    },
  },

  ready: function() {
    this.title = 'Bluetooth Settings';
  },

  pairDevice: function(e) {
    var device = this.devices[e.path[2].dataIndex];
    device.classValue = this.getValueForDeviceClass(device.class);

    // Send device info to the WebUI.
    chrome.send('requestBluetoothPair', [device]);
  },

  discoverDevice: function(e) {
    var device = this.devices[e.path[2].dataIndex];
    device.classValue = this.getValueForDeviceClass(device.class);

    // Send device info to WebUI.
    chrome.send('requestBluetoothDiscover', [device]);
  },

  appendNewDevice: function() {
    this.push('devices', new BluetoothDevice());
  },

  getValueForDeviceClass: function(classValue) {
    for (var i = 0; i < this.deviceClassOptions.length; ++i) {
      if (this.deviceClassOptions[i].text == classValue)
        return this.deviceClassOptions[i].value;
    }
    return 0;
  },
});
