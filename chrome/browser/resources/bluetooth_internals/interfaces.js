// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for Mojo interface helpers, served from
 *     chrome://bluetooth-internals/.
 */

cr.define('interfaces', function() {
  /**
   * Imports Mojo interfaces and adds them to window.interfaces.
   * @return {Promise}
   */
  function importInterfaces() {
    return importModules([
      'content/public/renderer/frame_interfaces',
      'device/bluetooth/public/interfaces/adapter.mojom',
      'device/bluetooth/public/interfaces/device.mojom',
      'mojo/public/js/connection',
    ]).then(function([frameInterfaces, bluetoothAdapter, bluetoothDevice,
        connection]) {
      Object.assign(interfaces, {
        BluetoothAdapter: bluetoothAdapter,
        BluetoothDevice: bluetoothDevice,
        Connection: connection,
        FrameInterfaces: frameInterfaces,
      });
    });
  }

  return {
    BluetoothAdapter: {},
    BluetoothDevice: {},
    Connection: {},
    FrameInterfaces: {},
    importInterfaces: importInterfaces,
  };
});
