// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for Mojo interface helpers, served from
 *     chrome://bluetooth-internals/.
 */

cr.define('interfaces', function() {
  /**
   * Overriden by tests to give them a chance to setup a fake Mojo browser proxy
   * before any other code executes.
   * @return {!Promise} A promise firing once necessary setup has been completed.
   */
  var setupFn = window.setupFn || function() {
    return Promise.resolve();
  };

  /**
   * Sets up Mojo interfaces and adds them to window.interfaces.
   * @return {Promise}
   */
  function setupInterfaces() {
    return setupFn().then(function() {
      return importModules([
               'content/public/renderer/frame_interfaces',
               'device/bluetooth/public/interfaces/adapter.mojom',
               'device/bluetooth/public/interfaces/device.mojom',
               'mojo/public/js/bindings',
             ])
          .then(function(
              [frameInterfaces, bluetoothAdapter, bluetoothDevice, bindings]) {
            interfaces.BluetoothAdapter = bluetoothAdapter;
            interfaces.BluetoothDevice = bluetoothDevice;
            interfaces.Bindings = bindings;
            interfaces.FrameInterfaces = frameInterfaces;
          });
    });
  }

  return {
    setupInterfaces: setupInterfaces,
  };
});
