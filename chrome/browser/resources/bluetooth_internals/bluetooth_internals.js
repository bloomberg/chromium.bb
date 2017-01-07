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
  /** @const */ var Snackbar = snackbar.Snackbar;
  /** @const */ var SnackbarType = snackbar.SnackbarType;

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

  /** @type {!Map<string, !interfaces.BluetoothDevice.DevicePtr>} */
  var deviceAddressToProxy = new Map();

  /** @type {!device_collection.DeviceCollection} */
  devices = new device_collection.DeviceCollection([]);

  /** @type {devices_page.DevicesPage} */
  var devicesPage = null;

  /** @type {interfaces.BluetoothAdapter.DiscoverySession.ptrClass} */
  var discoverySession = null;

  /** @type {boolean} */
  var userRequestedScanStop = false;

  function handleInspect(event) {
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
      var deviceInfo = devices.getByAddress(address);
      if (!deviceInfo) {
        // Device no longer in list, so drop the connection.
        deviceProxy.disconnect();
        return;
      }

      deviceAddressToProxy.set(address, deviceProxy);
      devices.updateConnectionStatus(
          address, device_collection.ConnectionStatus.CONNECTED);
      Snackbar.show(deviceInfo.name_for_display + ': Connected',
          SnackbarType.SUCCESS);

      // Fetch services asynchronously.
      return deviceProxy.getServices();
    }).then(function(response) {
      if (!response) return;

      var deviceInfo = devices.getByAddress(address);
      deviceInfo.services = response.services;
      devices.addOrUpdate(deviceInfo);
    }).catch(function(error) {
      // If a connection error occurs while fetching the services, the proxy
      // reference must be removed.
      var proxy = deviceAddressToProxy.get(address);
      if (proxy) {
        proxy.disconnect();
        deviceAddressToProxy.delete(address);
      }

      devices.updateConnectionStatus(
          address, device_collection.ConnectionStatus.DISCONNECTED);

      var deviceInfo = devices.getByAddress(address);
      Snackbar.show(deviceInfo.name_for_display + ': ' + error.message,
          SnackbarType.ERROR, 'Retry', function() { handleInspect(event); });
    });
  }

  function updateStoppedDiscoverySession() {
    devicesPage.setScanStatus(devices_page.ScanStatus.OFF);
    discoverySession.ptr.reset();
    discoverySession = null;
  }

  function setupAdapterSystem(response) {
    console.log('adapter', response.info);

    adapterBroker.addEventListener('adapterchanged', function(event) {
      if (event.detail.property == adapter_broker.AdapterProperty.DISCOVERING &&
          !event.detail.value && !userRequestedScanStop && discoverySession) {
        updateStoppedDiscoverySession();
        Snackbar.show(
            'Discovery session ended unexpectedly', SnackbarType.WARNING);
      }
    });
  }

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
    devicesPage.pageDiv.addEventListener('inspectpressed', handleInspect);

    devicesPage.pageDiv.addEventListener('scanpressed', function(event) {
      if (discoverySession && discoverySession.ptr.isBound()) {
        userRequestedScanStop = true;
        devicesPage.setScanStatus(devices_page.ScanStatus.STOPPING);

        discoverySession.stop().then(function(response) {
          if (response.success) {
            updateStoppedDiscoverySession();
            userRequestedScanStop = false;
            return;
          }

          devicesPage.setScanStatus(devices_page.ScanStatus.ON);
          Snackbar.show(
              'Failed to stop discovery session', SnackbarType.ERROR);
          userRequestedScanStop = false;
        });

        return;
      }

      devicesPage.setScanStatus(devices_page.ScanStatus.STARTING);
      adapterBroker.startDiscoverySession().then(function(session) {
        discoverySession = session;

        discoverySession.ptr.setConnectionErrorHandler(function() {
          updateStoppedDiscoverySession();
          Snackbar.show('Discovery session ended', SnackbarType.WARNING);
        });

        devicesPage.setScanStatus(devices_page.ScanStatus.ON);
      }).catch(function(error) {
        devicesPage.setScanStatus(devices_page.ScanStatus.OFF);
        Snackbar.show('Failed to start discovery session', SnackbarType.ERROR);
        console.error(error);
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
      .then(setupAdapterSystem)
      .then(function() { return adapterBroker.getDevices(); })
      .then(setupDeviceSystem)
      .catch(function(error) {
        Snackbar.show(error.message, SnackbarType.ERROR);
        console.error(error);
      });
  }

  return {
    initializeViews: initializeViews
  };
});

document.addEventListener(
    'DOMContentLoaded', bluetooth_internals.initializeViews);
