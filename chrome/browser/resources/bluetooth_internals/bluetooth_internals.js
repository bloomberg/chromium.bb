// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for bluetooth_internals.html, served from
 *     chrome://bluetooth-internals/.
 */

/**
 * The implementation of AdapterClient in
 *     device/bluetooth/public/interfaces/adapter.mojom.
 */
var AdapterClient = function() {};
AdapterClient.prototype = {
  /**
   * Prints added device to console.
   * @param {!bluetoothDevice.DeviceInfo} device
   */
  deviceAdded: function(device) { console.log('Device added', device); },

  /**
   * Prints removed device to console.
   * @param {!bluetoothDevice.DeviceInfo} device
   */
  deviceRemoved: function(device) { console.log('Device removed', device); }
};

(function() {
  var adapter, adapterClient, bluetoothAdapter, bluetoothDevice, connection;

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
      console.log('Loaded modules');

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

  /**
   * Prints device info from the device service.
   * @param {!bluetoothDevice.DeviceInfo} deviceInfo the device for which to
   *     get the information.
   * @return {!Promise} resolves if device service is retrieved, rejects
   *     otherwise.
   */
  function logDevice(deviceInfo) {
    return adapter.getDevice(deviceInfo.address).then(function(response) {
        var deviceHandle = response.device;
        if (!deviceHandle) {
          throw new Error(deviceInfo.name_for_display + ' cannot be found.');
        }

        var device = connection.bindHandleToProxy(
              deviceHandle, bluetoothDevice.Device);
        return device.getInfo();
      }).then(function(response) {
        console.log(deviceInfo.name_for_display, response.info);
      });
  }

  document.addEventListener('DOMContentLoaded', function() {
    initializeProxies()
      .then(function() { return adapter.getInfo(); })
      .then(function(response) { console.log('adapter', response.info); })
      .then(function() { return adapter.getDevices(); })
      .then(function(response) {
        var devices = response.devices;
        console.log('devices', devices.length);

        return Promise.all(devices.map(logDevice));
      })
      .catch(function(error) { console.error(error); });
  });
})();
