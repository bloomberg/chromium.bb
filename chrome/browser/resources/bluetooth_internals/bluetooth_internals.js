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
   * @param {!Object} device the device that was added
   */
  deviceAdded: function(device) {
    console.log('Device added');
    console.log(device);
  },

  /**
   * Prints removed device to console.
   * @param {!Object} device the device that was removed
   */
  deviceRemoved: function(device) {
    console.log('Device removed');
    console.log(device);
  }
};

(function() {
  var adapter, adapterClient;

  /**
   * TODO: Move to shared location. See crbug.com/652361.
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
      'mojo/public/js/connection',
    ]).then(function([frameInterfaces, bluetoothAdapter, connection]) {
      console.log('Loaded modules');

      // Hook up the instance properties.
      AdapterClient.prototype.__proto__ =
          bluetoothAdapter.AdapterClient.stubClass.prototype;

      var adapterFactory = connection.bindHandleToProxy(
          frameInterfaces.getInterface(bluetoothAdapter.AdapterFactory.name),
          bluetoothAdapter.AdapterFactory);

      // Get an Adapter service.
      return adapterFactory.getAdapter().then(function(response) {
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
    });
  }

  document.addEventListener('DOMContentLoaded', function() {
    initializeProxies()
      .then(function() { return adapter.getDevices(); })
      .then(function(response) { console.log(response.devices); })
      .catch(function(error) { console.error(error); });
  });
})();
