// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for chrome://usb-internals
 */

/**
 * Test fixture for testing async methods of cr.js.
 * @constructor
 * @extends testing.Test
 */
function UsbInternalsTest() {
  this.setupResolver = new PromiseResolver();
  this.deviceManagerGetDevicesResolver = new PromiseResolver();
}

UsbInternalsTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: 'chrome://usb-internals',

  /** @override */
  isAsync: true,

  /** @override */
  runAccessibilityChecks: false,

  /** @override */
  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
    '//ui/webui/resources/js/promise_resolver.js',
    '//ui/webui/resources/js/cr.js',
    '//ui/webui/resources/js/util.js',
    '//chrome/test/data/webui/test_browser_proxy.js',
  ],

  preLoad: function() {
    /** @implements {mojom.UsbInternalsPageHandlerProxy} */
    class FakePageHandlerProxy extends TestBrowserProxy {
      constructor() {
        super([
          'bindUsbDeviceManagerInterface',
          'bindTestInterface',
        ]);
      }

      async bindUsbDeviceManagerInterface(deviceManagerRequest) {
        this.methodCalled(
            'bindUsbDeviceManagerInterface', deviceManagerRequest);
        this.deviceManager = new FakeDeviceManagerProxy();
        this.deviceManager.addTestDevice(
            FakeDeviceManagerProxy.fakeDeviceInfo(0));
        this.deviceManager.addTestDevice(
            FakeDeviceManagerProxy.fakeDeviceInfo(1));
        this.deviceManagerBinding_ =
            new device.mojom.UsbDeviceManager(this.deviceManager);
        this.deviceManagerBinding_.bindHandle(deviceManagerRequest.handle);
      }

      async bindTestInterface(testDeviceManagerRequest) {
        this.methodCalled('bindTestInterface', testDeviceManagerRequest);
      }
    }

    /** @implements {device.mojom.UsbDeviceManagerProxy} */
    class FakeDeviceManagerProxy extends TestBrowserProxy {
      constructor() {
        super([
          'enumerateDevicesAndSetClient',
          'getDevice',
          'getDevices',
          'checkAccess',
          'openFileDescriptor',
          'setClient',
        ]);

        this.devices = [];
      }

      addTestDevice(device) {
        this.devices.push(device);
      }

      /**
       * Creates a fake device use given number.
       * @param {number} num
       * @return {!Object}
       */
      static fakeDeviceInfo(num) {
        return {
          guid: `abcdefgh-ijkl-mnop-qrst-uvwxyz12345${num}`,
          usbVersionMajor: 2,
          usbVersionMinor: 0,
          usbVersionSubminor: 2,
          classCode: 0,
          subclassCode: 0,
          protocolCode: 0,
          busNumber: num,
          portNumber: num,
          vendorId: 0x1050 + num,
          productId: 0x17EF + num,
          deviceVersionMajor: 3,
          deviceVersionMinor: 2,
          deviceVersionSubminor: 1,
          manufacturerName: undefined,
          productName: undefined,
          serialNumber: undefined,
          webusbLandingPage: undefined,
          activeConfiguration: 1,
          configurations: [],
        };
      }

      async enumerateDevicesAndSetClient() {}

      async getDevice() {}

      async getDevices() {
        this.methodCalled('getDevices');
        return Promise.resolve({results: this.devices});
      }

      async checkAccess() {}

      async openFileDescriptor() {}

      async setClient() {}
    }

    window.deviceListCompleteFn = () => {
      this.deviceManagerGetDevicesResolver.resolve();
      return Promise.resolve();
    };

    window.setupFn = () => {
      this.pageHandlerInterceptor = new MojoInterfaceInterceptor(
          mojom.UsbInternalsPageHandler.$interfaceName);
      this.pageHandlerInterceptor.oninterfacerequest = (e) => {
        this.pageHandler = new FakePageHandlerProxy();
        this.pageHandlerBinding_ =
            new mojom.UsbInternalsPageHandler(this.pageHandler);
        this.pageHandlerBinding_.bindHandle(e.handle);
      };
      this.pageHandlerInterceptor.start();

      this.setupResolver.resolve();
      return Promise.resolve();
    };
  },
};

TEST_F('UsbInternalsTest', 'WebUITest', function() {
  let pageHandler;

  // Before tests are run, make sure setup completes.
  let setupPromise = this.setupResolver.promise.then(() => {
    pageHandler = this.pageHandler;
  });

  let deviceManagerGetDevicesPromise =
      this.deviceManagerGetDevicesResolver.promise;

  suite('UsbtoothInternalsUITest', function() {
    const EXPECT_DEVICES_NUM = 2;

    suiteSetup(function() {
      return setupPromise.then(function() {
        return Promise.all([
          pageHandler.whenCalled('bindUsbDeviceManagerInterface'),
          pageHandler.deviceManager.whenCalled('getDevices'),
        ]);
      });
    });

    teardown(function() {
      pageHandler.reset();
    });

    test('PageLoaded', async function() {
      // Totally 2 tables: 'TestDevice' table and 'Device' table.
      const tables = document.querySelectorAll('table');
      expectEquals(2, tables.length);

      // Only 2 tabs after loading page.
      const tabs = document.querySelectorAll('tab');
      expectEquals(2, tabs.length);
      const tabpanels = document.querySelectorAll('tabpanel');
      expectEquals(2, tabpanels.length);

      // The second is the devices table, which has 8 columns.
      const devicesTable = document.querySelectorAll('table')[1];
      const columns = devicesTable.querySelector('thead')
                          .querySelector('tr')
                          .querySelectorAll('th');
      expectEquals(8, columns.length);

      await deviceManagerGetDevicesPromise;
      const devices = devicesTable.querySelectorAll('tbody tr');
      expectEquals(EXPECT_DEVICES_NUM, devices.length);
    });
  });

  // Run all registered tests.
  mocha.run();
});
