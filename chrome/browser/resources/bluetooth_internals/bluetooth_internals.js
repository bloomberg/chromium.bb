// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for bluetooth_internals.html, served from
 *     chrome://bluetooth-internals/.
 */

(function() {
  var adapter, adapterClient, bluetoothAdapter, bluetoothDevice, connection;

  var REMOVED_CSS = 'removed';

  /*
   * Data model for a cached device.
   * @constructor
   * @param {!bluetoothDevice.DeviceInfo} info
   */
  var Device = function(info) { this.info = info; };

  /**
   * The implementation of AdapterClient in
   *     device/bluetooth/public/interfaces/adapter.mojom. This also manages the
   *     client-side collection of devices.
   * @constructor
   */
  var AdapterClient = function() { this.devices_ = new Map(); };
  AdapterClient.prototype = {
    /**
     * Caches the device info and updates the device list.
     * @param {!bluetoothDevice.DeviceInfo} deviceInfo
     */
    deviceAdded: function(deviceInfo) {
      if (this.devices_.has(deviceInfo.address)) {
        var deviceElement = $(deviceInfo.address);
        deviceElement.classList.remove(REMOVED_CSS);
      } else {
        this.devices_.set(deviceInfo.address, new Device(deviceInfo));

        var deviceRowTemplate = $('device-row-template');
        var deviceRow = document.importNode(
            deviceRowTemplate.content.children[0], true /* deep */);
        deviceRow.id = deviceInfo.address;

        var deviceList = $('device-list');
        deviceList.appendChild(deviceRow);
      }

      this.deviceChanged(deviceInfo);
    },

    /**
     * Marks device as removed.
     * @param {!bluetoothDevice.DeviceInfo} deviceInfo
     */
    deviceRemoved: function(deviceInfo) {
      $(deviceInfo.address).classList.add(REMOVED_CSS);
    },

    /**
     * Updates cached device and updates the device list.
     * @param {!bluetoothDevice.DeviceInfo} deviceInfo
     */
    deviceChanged: function(deviceInfo) {
      console.log(new Date(), deviceInfo);

      assert(this.devices_.has(deviceInfo.address), 'Device does not exist.');

      this.devices_.get(deviceInfo.address).info = deviceInfo;

      var deviceRow = $(deviceInfo.address);
      deviceRow.querySelector('.device-name').textContent =
          deviceInfo.name_for_display;
      deviceRow.querySelector('.device-address').textContent =
          deviceInfo.address;

      var rssi = (deviceInfo.rssi && deviceInfo.rssi.value) ||
          deviceRow.querySelector('.device-rssi').textContent;
      deviceRow.querySelector('.device-rssi').textContent = rssi;
    }
  };

  /**
   * TODO(crbug.com/652361): Move to shared location.
   * Helper to convert callback-based define() API to a promise-based API.
   * @param {!Array<string>} moduleNames
   * @return {!Promise}
   */
  function importModules(moduleNames) {
    return new Promise(function(resolve, reject) {
      define(moduleNames, function(var_args) {
        resolve(Array.prototype.slice.call(arguments, 0));
      });
    });
  }

  /**
   * Initializes Mojo proxies for page and Bluetooth services.
   * @return {!Promise} resolves if adapter is acquired, rejects if Bluetooth
   *     is not supported.
   */
  function initializeProxies() {
    return importModules([
      'content/public/renderer/frame_interfaces',
      'device/bluetooth/public/interfaces/adapter.mojom',
      'device/bluetooth/public/interfaces/device.mojom',
      'mojo/public/js/connection',
    ]).then(function([frameInterfaces, ...modules]) {
      // Destructure here to assign global variables.
      [bluetoothAdapter, bluetoothDevice, connection] = modules;

      // Hook up the instance properties.
      AdapterClient.prototype.__proto__ =
          bluetoothAdapter.AdapterClient.stubClass.prototype;

      var adapterFactory = connection.bindHandleToProxy(
          frameInterfaces.getInterface(bluetoothAdapter.AdapterFactory.name),
          bluetoothAdapter.AdapterFactory);

      // Get an Adapter service.
      return adapterFactory.getAdapter();
    }).then(function(response) {
      if (!response.adapter) {
        throw new Error('Bluetooth Not Supported on this platform.');
      }

      adapter = connection.bindHandleToProxy(response.adapter,
                                             bluetoothAdapter.Adapter);

      // Create a message pipe and bind one end to client
      // implementation and the other to the Adapter service.
      adapterClient = new AdapterClient();
      adapter.setClient(connection.bindStubDerivedImpl(adapterClient));
    });
  }

  document.addEventListener('DOMContentLoaded', function() {
    initializeProxies()
      .then(function() { return adapter.getInfo(); })
      .then(function(response) { console.log('adapter', response.info); })
      .then(function() { return adapter.getDevices(); })
      .then(function(response) {
        response.devices.forEach(adapterClient.deviceAdded, adapterClient);
      })
      .catch(function(error) { console.error(error); });
  });
})();
