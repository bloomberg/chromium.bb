// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DeviceCollection, served from
 *     chrome://bluetooth-internals/.
 */

cr.define('device_collection', function() {
  /**
   * Enum of connection status for a device. Used for
   * DeviceCollection.updateConnectionStatus which sets the connectionStatus
   * on the DeviceInfo object. New DeviceInfo objects have a DISCONNECTED status
   * by default.
   * @enum {number}
   */
  var ConnectionStatus = {
    DISCONNECTED: 0,
    CONNECTING: 1,
    CONNECTED: 2,
  };

  /**
   * Collection of devices. Extends ArrayDataModel which provides a set of
   * functions and events that notifies observers when the collection changes.
   * @constructor
   * @param {!Array<!bluetooth.mojom.DeviceInfo>} array The starting
   *     collection of devices.
   * @extends {cr.ui.ArrayDataModel}
   */
  var DeviceCollection = function(array) {
    cr.ui.ArrayDataModel.call(this, array);
  };
  DeviceCollection.prototype = {
    __proto__: cr.ui.ArrayDataModel.prototype,

    /**
     * Finds the Device in the collection with the matching address.
     * @param {string} address
     */
    getByAddress: function(address) {
      for (var i = 0; i < this.length; i++) {
        var device = this.item(i);
        if (address == device.address) {
          return device;
        }
      }
      return null;
    },

    /**
     * Adds or updates a Device with new DeviceInfo.
     * @param {!bluetooth.mojom.DeviceInfo} deviceInfo
     */
    addOrUpdate: function(deviceInfo) {
      deviceInfo.removed = false;
      var oldDeviceInfo = this.getByAddress(deviceInfo.address);

      if (oldDeviceInfo) {
        // Update rssi if it's valid
        var rssi = (deviceInfo.rssi && deviceInfo.rssi.value) ||
            (oldDeviceInfo.rssi && oldDeviceInfo.rssi.value);

        // The connectionStatus and connectionMessage properties may not exist
        // on |deviceInfo|. The rssi property may be null, so it must be
        // re-assigned.
        Object.assign(oldDeviceInfo, deviceInfo);
        oldDeviceInfo.rssi = {value: rssi};
        this.updateIndex(this.indexOf(oldDeviceInfo));
      } else {
        deviceInfo.connectionStatus = ConnectionStatus.DISCONNECTED;
        this.push(deviceInfo);
      }
    },

    /**
     * Marks the Device as removed.
     * @param {!bluetooth.mojom.DeviceInfo} deviceInfo
     */
    remove: function(deviceInfo) {
      var device = this.getByAddress(deviceInfo.address);
      assert(device, 'Device does not exist.');
      device.removed = true;
      this.updateIndex(this.indexOf(device));
    },

    /**
     * Updates the device connection status.
     * @param {string} address The address of the device.
     * @param {number} status The new connection status.
     */
    updateConnectionStatus: function(address, status) {
      var device = assert(this.getByAddress(address), 'Device does not exist');
      device.connectionStatus = status;
      this.updateIndex(this.indexOf(device));
    },
  };

  return {
    ConnectionStatus: ConnectionStatus,
    DeviceCollection: DeviceCollection,
  };
});
