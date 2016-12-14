// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for bluetooth_internals.html, served from
 *     chrome://bluetooth-internals/.
 */

// Expose for testing.
var adapterBroker = null;
var devices = null;
var sidebarObj = null;

cr.define('bluetooth_internals', function() {
  /** @const */ var DevicesPage = devices_page.DevicesPage;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;

  /**
   * Observer for page changes. Used to update page title header.
   * @extends {cr.ui.pageManager.PageManager.Observer}
   */
  var PageObserver = function() {};

  PageObserver.prototype = {
    __proto__: PageManager.Observer.prototype,

    updateHistory: function(path) {
      window.location.hash = '#' + path;
    },

    /**
     * Sets the page title. Called by PageManager.
     * @override
     * @param {string} title
     */
    updateTitle: function(title) {
      document.querySelector('.page-title').textContent = title;
    },
  };

  /** @type {!Map<string, !interfaces.BluetoothDevice.Device.proxyClass>} */
  var deviceAddressToProxy = new Map();

  /** @type {!device_collection.DeviceCollection} */
  devices = new device_collection.DeviceCollection([]);

  /** @type {devices_page.DevicesPage} */
  var devicesPage = null;

  function setupDeviceSystem(response) {
    // Hook up device collection events.
    adapterBroker.addEventListener('deviceadded', function(event) {
      devices.addOrUpdate(event.detail.deviceInfo);
    });
    adapterBroker.addEventListener('devicechanged', function(event) {
      devices.addOrUpdate(event.detail.deviceInfo);
    });
    adapterBroker.addEventListener('deviceremoved', function(event) {
      devices.remove(event.detail.deviceInfo);
    });

    response.devices.forEach(devices.addOrUpdate, devices /* this */);

    devicesPage.setDevices(devices);
    devicesPage.pageDiv.addEventListener('inspectpressed', function() {
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
        if (!response) return;

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
  }

  function setupPages() {
    sidebarObj = new window.sidebar.Sidebar($('sidebar'));
    $('menu-btn').addEventListener('click', function() { sidebarObj.open(); });
    PageManager.addObserver(sidebarObj);
    PageManager.addObserver(new PageObserver());

    devicesPage = new DevicesPage();
    PageManager.register(devicesPage);

    // Set up hash-based navigation.
    window.addEventListener('hashchange', function() {
      PageManager.showPageByName(window.location.hash.substr(1));
    });

    if (!window.location.hash) {
      PageManager.showPageByName(devicesPage.name);
      return;
    }

    PageManager.showPageByName(window.location.hash.substr(1));
  }

  function initializeViews() {
    setupPages();

    adapter_broker.getAdapterBroker()
      .then(function(broker) { adapterBroker = broker; })
      .then(function() { return adapterBroker.getInfo(); })
      .then(function(response) { console.log('adapter', response.info); })
      .then(function() { return adapterBroker.getDevices(); })
      .then(setupDeviceSystem)
      .catch(function(error) { console.error(error); });
  }

  return {
    initializeViews: initializeViews
  };
});

document.addEventListener(
    'DOMContentLoaded', bluetooth_internals.initializeViews);
