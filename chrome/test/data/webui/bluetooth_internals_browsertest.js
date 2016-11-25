// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for chrome://bluetooth-internals
 */

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../';

/**
 * Test fixture for BluetoothInternals WebUI testing.
 * @constructor
 * @extends testing.Test
 */
function BluetoothInternalsTest() {
  this.adapterFactory = null;
  this.setupResolver = new PromiseResolver();
}

BluetoothInternalsTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: 'chrome://bluetooth-internals',

  /** @override */
  isAsync: true,

  /** @override */
  runAccessibilityChecks: false,

  /** @override */
  extraLibraries: [
    ROOT_PATH + 'third_party/mocha/mocha.js',
    ROOT_PATH + 'chrome/test/data/webui/mocha_adapter.js',
    ROOT_PATH + 'ui/webui/resources/js/promise_resolver.js',
    ROOT_PATH + 'ui/webui/resources/js/cr.js',
    ROOT_PATH + 'ui/webui/resources/js/util.js',
    ROOT_PATH + 'chrome/test/data/webui/settings/test_browser_proxy.js',
  ],

  preLoad: function() {
    // A function that is called from chrome://bluetooth-internals to allow this
    // test to replace the real Mojo browser proxy with a fake one, before any
    // other code runs.
    window.setupFn = function() {
      return importModules([
        'content/public/renderer/frame_interfaces',
        'device/bluetooth/public/interfaces/adapter.mojom',
        'device/bluetooth/public/interfaces/device.mojom',
        'mojo/public/js/bindings',
        'mojo/public/js/connection',
      ]).then(function([frameInterfaces, adapter, device, bindings,
          connection]) {
        /**
          * A test adapter factory proxy for the chrome://bluetooth-internals
          * page.
          *
          * @constructor
          * @extends {TestBrowserProxyBase}
          */
        var TestAdapterFactoryProxy = function() {
          settings.TestBrowserProxy.call(this, [
            'getAdapter',
          ]);

          this.adapter = new TestAdapter();
          this.adapterHandle_ = connection.bindStubDerivedImpl(this.adapter);
        };

        TestAdapterFactoryProxy.prototype = {
          __proto__: settings.TestBrowserProxy.prototype,
          getAdapter: function() {
            this.methodCalled('getAdapter');

            // Create message pipe bound to TestAdapter.
            return Promise.resolve({
              adapter: this.adapterHandle_,
            });
          }
        };

        /**
          * A test adapter for the chrome://bluetooth-internals page.
          * Must be used to create message pipe handle from test code.
          *
          * @constructor
          * @extends {adapter.Adapter.stubClass}
          */
        var TestAdapter = function() {
          this.proxy = new TestAdapterProxy();
        };

        TestAdapter.prototype = {
          __proto__: adapter.Adapter.stubClass.prototype,
          getInfo: function() { return this.proxy.getInfo(); },
          getDevices: function() { return this.proxy.getDevices(); },
          setClient: function(client) { return this.proxy.setClient(client); }
        };

        /**
          * A test adapter proxy for the chrome://bluetooth-internals page.
          *
          * @constructor
          * @extends {TestBrowserProxyBase}
          */
        var TestAdapterProxy = function() {
          settings.TestBrowserProxy.call(this, [
            'getInfo',
            'getDevices',
            'setClient',
          ]);

          this.adapterInfo_ = null;
          this.devices_ = [];
        };

        TestAdapterProxy.prototype = {
          __proto__: settings.TestBrowserProxy.prototype,

          getInfo: function() {
            this.methodCalled('getInfo');
            return Promise.resolve({info: this.adapterInfo_});
          },

          getDevices: function() {
            this.methodCalled('getDevices');
            return Promise.resolve({devices: this.devices_});
          },

          setClient: function(client) {
            this.methodCalled('setClient', client);
          },

          setTestAdapter: function(adapterInfo) {
            this.adapterInfo_ = adapterInfo;
          },

          setTestDevices: function(devices) {
            this.devices_ = devices;
          }
        };

        frameInterfaces.addInterfaceOverrideForTesting(
          adapter.AdapterFactory.name,
          function(handle) {
            var stub = connection.bindHandleToStub(
                handle, adapter.AdapterFactory);
            this.adapterFactory = new TestAdapterFactoryProxy();
            bindings.StubBindings(stub).delegate = this.adapterFactory;

            this.setupResolver.resolve();
          }.bind(this));
      }.bind(this));
    }.bind(this);
  },
};

// Times out. See https://crbug.com/667970.
TEST_F('BluetoothInternalsTest', 'DISABLED_Startup_BluetoothInternals',
    function() {
  var fakeAdapterInfo = {
    address: '02:1C:7E:6A:11:5A',
    discoverable: false,
    discovering: false,
    initialized: true,
    name: 'computer.example.com-0',
    powered: true,
    present: true,
  };

  var fakeDeviceInfo1 = {
    address: "AA:AA:84:96:92:84",
    name: "AAA",
    name_for_display: "AAA",
    rssi: {value: -40},
    services: []
  };

  var fakeDeviceInfo2 = {
    address: "BB:BB:84:96:92:84",
    name: "BBB",
    name_for_display: "BBB",
    rssi: null,
    services: []
  };

  var fakeDeviceInfo3 = {
    address: "CC:CC:84:96:92:84",
    name: "CCC",
    name_for_display: "CCC",
  };

  var adapterFactory = null;

  // Before tests are run, make sure setup completes.
  var setupPromise = this.setupResolver.promise.then(function() {
    adapterFactory = this.adapterFactory;
  }.bind(this));


  suite('BluetoothInternalsUITest', function() {
    var EXPECTED_DEVICES = 2;

    suiteSetup(function() {
      return setupPromise.then(function() {
        adapterFactory.adapter.proxy.setTestDevices([
          fakeDeviceInfo1,
          fakeDeviceInfo2
        ]);
        adapterFactory.adapter.proxy.setTestAdapter(fakeAdapterInfo);

        return Promise.all([
          adapterFactory.whenCalled('getAdapter'),
          adapterFactory.adapter.proxy.whenCalled('getInfo'),
          adapterFactory.adapter.proxy.whenCalled('getDevices'),
          adapterFactory.adapter.proxy.whenCalled('setClient'),
        ]);
      });
    });

    setup(function() {
      devices.splice(0, devices.length);
      adapterBroker.adapterClient_.deviceAdded(fakeDeviceInfo1);
      adapterBroker.adapterClient_.deviceAdded(fakeDeviceInfo2);
    });

    teardown(function() {
      adapterFactory.reset();
    });

    /**
     * Updates device info and verifies the contents of the device table.
     * @param {!device_collection.DeviceInfo} deviceInfo
     */
    function changeDevice(deviceInfo) {
      var deviceRow = document.querySelector('#' + escapeDeviceAddress(
          deviceInfo.address));
      var nameForDisplayColumn = deviceRow.children[0];
      var addressColumn = deviceRow.children[1];
      var rssiColumn = deviceRow.children[2];
      var servicesColumn = deviceRow.children[3];

      expectTrue(!!nameForDisplayColumn);
      expectTrue(!!addressColumn);
      expectTrue(!!rssiColumn);
      expectTrue(!!servicesColumn);

      adapterBroker.adapterClient_.deviceChanged(deviceInfo);

      expectEquals(deviceInfo.name_for_display,
                   nameForDisplayColumn.textContent);
      expectEquals(deviceInfo.address, addressColumn.textContent);

      if (deviceInfo.rssi) {
        expectEquals(String(deviceInfo.rssi.value), rssiColumn.textContent);
      }

      if (deviceInfo.services) {
        expectEquals(String(deviceInfo.services.length),
                     servicesColumn.textContent);
      } else {
        expectEquals('Unknown', servicesColumn.textContent);
      }
    }

    /**
     * Escapes colons in a device address for CSS formatting.
     * @param {string} address
     */
    function escapeDeviceAddress(address) {
      return address.replace(/:/g, '\\:');
    }

    /**
     * Expects whether device with |address| is removed.
     * @param {string} address
     * @param {boolean} expectRemoved
     */
    function expectDeviceRemoved(address, expectRemoved) {
      var removedRow = document.querySelector(
          '#' + escapeDeviceAddress(address));

      expectEquals(expectRemoved, removedRow.classList.contains('removed'));
    }

    /**
     * Tests whether a device is added successfully and not duplicated.
     */
    test('DeviceAdded', function() {
      var devices = document.querySelectorAll('#device-table tbody tr');
      expectEquals(EXPECTED_DEVICES, devices.length);

      // Copy device info because device collection will not copy this object.
      var infoCopy = Object.assign({}, fakeDeviceInfo3);
      adapterBroker.adapterClient_.deviceAdded(infoCopy);

      // Same device shouldn't appear twice.
      adapterBroker.adapterClient_.deviceAdded(infoCopy);

      devices = document.querySelectorAll('#device-table tbody tr');
      expectEquals(EXPECTED_DEVICES + 1, devices.length);
    });

    /**
     * Tests whether a device is marked properly as removed.
     */
    test('DeviceSetToRemoved', function() {
      var devices = document.querySelectorAll('#device-table tbody tr');
      expectEquals(EXPECTED_DEVICES, devices.length);
      adapterBroker.adapterClient_.deviceRemoved(fakeDeviceInfo2);

      // The number of rows shouldn't change.
      devices = document.querySelectorAll('#device-table tbody tr');
      expectEquals(EXPECTED_DEVICES, devices.length);

      expectDeviceRemoved(fakeDeviceInfo2.address, true);
    });

    /**
     * Tests whether a changed device updates the device table properly.
     */
    test('DeviceChanged', function() {
      var devices = document.querySelectorAll('#device-table tbody tr');
      expectEquals(EXPECTED_DEVICES, devices.length);

      // Copy device info because device collection will not copy this object.
      var newDeviceInfo = Object.assign({}, fakeDeviceInfo1);
      newDeviceInfo.name_for_display = 'DDDD';
      newDeviceInfo.rssi = { value: -20 };
      newDeviceInfo.services = ['service1', 'service2', 'service3'];

      changeDevice(newDeviceInfo);
    });

    /**
     * Tests the entire device cycle, added -> updated -> removed -> re-added.
     */
    test('DeviceUpdateCycle', function() {
      var devices = document.querySelectorAll('#device-table tbody tr');
      expectEquals(EXPECTED_DEVICES, devices.length);

      // Copy device info because device collection will not copy this object.
      var originalDeviceInfo = Object.assign({}, fakeDeviceInfo3);
      adapterBroker.adapterClient_.deviceAdded(originalDeviceInfo);

      var newDeviceInfo = Object.assign({}, fakeDeviceInfo3);
      newDeviceInfo.name_for_display = 'DDDD';
      newDeviceInfo.rssi = { value: -20 };
      newDeviceInfo.services = ['service1', 'service2', 'service3'];

      changeDevice(newDeviceInfo);
      changeDevice(originalDeviceInfo);

      adapterBroker.adapterClient_.deviceRemoved(originalDeviceInfo);
      expectDeviceRemoved(originalDeviceInfo.address, true);

      adapterBroker.adapterClient_.deviceAdded(originalDeviceInfo);
      expectDeviceRemoved(originalDeviceInfo.address, false);
    });

    test('DeviceAddedRssiCheck', function() {
      var devices = document.querySelectorAll('#device-table tbody tr');
      expectEquals(EXPECTED_DEVICES, devices.length);

      // Copy device info because device collection will not copy this object.
      var newDeviceInfo = Object.assign({}, fakeDeviceInfo3);
      adapterBroker.adapterClient_.deviceAdded(newDeviceInfo);

      var deviceRow = document.querySelector('#' + escapeDeviceAddress(
          newDeviceInfo.address));
      var rssiColumn = deviceRow.children[2];
      expectEquals('Unknown', rssiColumn.textContent);

      var newDeviceInfo1 = Object.assign({}, fakeDeviceInfo3);
      newDeviceInfo1.rssi = {value: -42};
      adapterBroker.adapterClient_.deviceChanged(newDeviceInfo1);
      expectEquals('-42', rssiColumn.textContent);

      // Device table should keep last valid rssi value.
      var newDeviceInfo2 = Object.assign({}, fakeDeviceInfo3);
      newDeviceInfo2.rssi = null;
      adapterBroker.adapterClient_.deviceChanged(newDeviceInfo2);
      expectEquals('-42', rssiColumn.textContent);

      var newDeviceInfo3 = Object.assign({}, fakeDeviceInfo3);
      newDeviceInfo3.rssi = {value: -17};
      adapterBroker.adapterClient_.deviceChanged(newDeviceInfo3);
      expectEquals('-17', rssiColumn.textContent);
    });
  });

  // Run all registered tests.
  mocha.run();
});
