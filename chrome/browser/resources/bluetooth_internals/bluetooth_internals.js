// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for bluetooth_internals.html, served from
 *     chrome://bluetooth-internals/.
 */

cr.define('bluetooth_internals', function() {

  /** @type {!Map<string, !interfaces.BluetoothDevice.Device.proxyClass>} */
  var deviceAddressToProxy = new Map();

  function initializeViews() {
    var adapterBroker = null;
    adapter_broker.getAdapterBroker()
      .then(function(broker) { adapterBroker = broker; })
      .then(function() { return adapterBroker.getInfo(); })
      .then(function(response) { console.log('adapter', response.info); })
      .then(function() { return adapterBroker.getDevices(); })
      .then(function(response) {
        // Hook up device collection events.
        var devices = new device_collection.DeviceCollection([]);
        adapterBroker.addEventListener('deviceadded', function(event) {
          devices.addOrUpdate(event.detail.deviceInfo);
        });
        adapterBroker.addEventListener('devicechanged', function(event) {
          devices.addOrUpdate(event.detail.deviceInfo);
        });
        adapterBroker.addEventListener('deviceremoved', function(event) {
          devices.remove(event.detail.deviceInfo);
        });

        response.devices.forEach(devices.addOrUpdate,
                                 devices /* this */);

        var deviceTable = new device_table.DeviceTable();

        deviceTable.addEventListener('inspectpressed', function(event) {
          // TODO(crbug.com/663470): Move connection logic to DeviceDetailsView
          // when it's added in chrome://bluetooth-internals.
          var address = event.detail.address;
          var proxy = deviceAddressToProxy.get(address);

          if (proxy) {
            // Device is already connected, so disconnect.
            proxy.disconnect();
            deviceAddressToProxy.delete(address);
            devices.updateConnectionStatus(
                address, device_collection.ConnectionStatus.DISCONNECTED);
            return;
          }

          devices.updateConnectionStatus(
              address, device_collection.ConnectionStatus.CONNECTING);
          adapterBroker.connectToDevice(address).then(function(deviceProxy) {
            if (!devices.getByAddress(address)) {
              // Device no longer in list, so drop the connection.
              deviceProxy.disconnect();
              return;
            }

            deviceAddressToProxy.set(address, deviceProxy);
            devices.updateConnectionStatus(
                address, device_collection.ConnectionStatus.CONNECTED);

            // Fetch services asynchronously.
            return deviceProxy.getServices();
          }).then(function(response) {
            var deviceInfo = devices.getByAddress(address);
            deviceInfo.services = response.services;
            devices.addOrUpdate(deviceInfo);
          }).catch(function(error) {
            devices.updateConnectionStatus(
                address,
                device_collection.ConnectionStatus.DISCONNECTED,
                error);
          });
        });

        deviceTable.setDevices(devices);
        document.body.appendChild(deviceTable);
      })
      .catch(function(error) { console.error(error); });
  }

  return {
    initializeViews: initializeViews
  };

});

document.addEventListener(
    'DOMContentLoaded', bluetooth_internals.initializeViews);
