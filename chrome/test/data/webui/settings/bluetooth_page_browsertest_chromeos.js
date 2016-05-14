// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for settings-bluetooth-page. */

GEN_INCLUDE(['settings_page_browsertest.js']);

var bluetoothPage = bluetoothPage || {};

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
*/
function SettingsBluetoothPageBrowserTest() {
}

SettingsBluetoothPageBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/advanced',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    '../fake_chrome_event.js',
    'fake_bluetooth.js',
    'fake_bluetooth_private.js'
  ]),

  /** @type {Bluetooth} */
  bluetoothApi_: undefined,

  /** @type {BluetoothPrivate} */
  bluetoothPrivateApi_: undefined,

  /** @override */
  preLoad: function() {
    SettingsPageBrowserTest.prototype.preLoad.call(this);
    settingsHidePagesByDefaultForTest = true;
    this.bluetoothApi_ = new settings.FakeBluetooth();
    this.bluetoothPrivateApi_ =
        new settings.FakeBluetoothPrivate(this.bluetoothApi_);
    // Set globals to override Settings Bluetooth Page apis.
    bluetoothPage.bluetoothApiForTest = this.bluetoothApi_;
    bluetoothPage.bluetoothPrivateApiForTest = this.bluetoothPrivateApi_;
  }
};

// Times out on debug builders and may time out on memory bots because
// the Settings page can take several seconds to load in a Release build
// and several times that in a Debug build. See https://crbug.com/558434.
GEN('#if defined(MEMORY_SANITIZER) || !defined(NDEBUG)');
GEN('#define MAYBE_Bluetooth DISABLED_Bluetooth');
GEN('#else');
GEN('#define MAYBE_Bluetooth Bluetooth');
GEN('#endif');

// Runs bluetooth tests.
TEST_F('SettingsBluetoothPageBrowserTest', 'MAYBE_Bluetooth', function() {
  // Differentiate |this| in the Test from |this| in suite() and test(), see
  // comment at the top of mocha_adapter.js.
  var self = this;

  var advanced = self.getPage('advanced');
  assertTrue(!!advanced);
  advanced.set('pageVisibility.bluetooth', true);
  Polymer.dom.flush();

  /** @type {!Array<!chrome.bluetooth.Device>} */ var fakeDevices_ = [
    {
      address: '10:00:00:00:00:01',
      name: 'FakePairedDevice1',
      paired: true,
      connected: true,
    },
    {
      address: '10:00:00:00:00:02',
      name: 'FakePairedDevice2',
      paired: true,
      connected: false,
      connecting: true,
    },
    {
      address: '00:00:00:00:00:01',
      name: 'FakeUnPairedDevice1',
      paired: false,
    },
    {
      address: '00:00:00:00:00:02',
      name: 'FakeUnPairedDevice2',
      paired: false,
    },
  ];

  suite('SettingsBluetoothPage', function() {
    test('enable', function() {
      assertFalse(self.bluetoothApi_.adapterState.powered);
      var bluetoothSection = self.getSection(advanced, 'bluetooth');
      assertTrue(!!bluetoothSection);
      var bluetooth =
          bluetoothSection.querySelector('settings-bluetooth-page');
      assertTrue(!!bluetooth);
      expectFalse(bluetooth.bluetoothEnabled);
      var enable = bluetooth.$.enableBluetooth;
      assertTrue(!!enable);
      expectFalse(enable.checked);
      // Test that tapping the enable checkbox changes its value and calls
      // bluetooth.setAdapterState with powered = false.
      MockInteractions.tap(enable);
      Polymer.dom.flush();
      expectTrue(enable.checked);
      expectTrue(self.bluetoothApi_.adapterState.powered);
      // Confirm that 'bluetoothEnabled' remains set to true.
      expectTrue(bluetooth.bluetoothEnabled);
      // Set 'bluetoothEnabled' directly and confirm that the checkbox
      // toggles.
      bluetooth.bluetoothEnabled = false;
      Polymer.dom.flush();
      expectFalse(enable.checked);
    });

    test('device list', function() {
      var bluetoothSection = self.getSection(advanced, 'bluetooth');
      var bluetooth =
          bluetoothSection.querySelector('settings-bluetooth-page');
      assertTrue(!!bluetooth);
      var deviceList = bluetooth.$.deviceList;
      assertTrue(!!deviceList);
      // Set enabled, with default (empty) device list.
      self.bluetoothApi_.setEnabled(true);
      Polymer.dom.flush();
      // Ensure that initially the 'no devices' element is visible.
      expectFalse(deviceList.hidden);
      var noDevices = deviceList.querySelector('.no-devices');
      assertTrue(!!noDevices);
      expectFalse(noDevices.hidden);
      // Set some devices (triggers onDeviceAdded events). 'no devices' element
      // should be hidden.
      self.bluetoothApi_.setDevicesForTest(fakeDevices_);
      Polymer.dom.flush();
      expectTrue(noDevices.hidden);
      // Confirm that there are two paired devices in the list.
      var devices = deviceList.querySelectorAll('bluetooth-device-list-item');
      assertEquals(2, devices.length);
      // Check the state of each device.
      assertTrue(devices[0].device.connected);
      assertFalse(devices[1].device.connected);
      assertTrue(devices[1].device.connecting);
    });

    test('device dialog', function() {
      var bluetoothSection = self.getSection(advanced, 'bluetooth');
      var bluetooth =
          bluetoothSection.querySelector('settings-bluetooth-page');
      assertTrue(!!bluetooth);
      self.bluetoothApi_.setEnabled(true);

      // Tap the 'add device' button.
      MockInteractions.tap(bluetooth.$$('.primary-button'));
      Polymer.dom.flush();
      // Ensure the dialog appears.
      var dialog = bluetooth.$.deviceDialog;
      assertTrue(!!dialog);
      assertTrue(dialog.$.dialog.opened);

      // Ensure the dialog has the expected devices.
      var devicesDiv = dialog.$$('#dialogDeviceList');
      assertTrue(!!devicesDiv);
      var devices = devicesDiv.querySelectorAll('bluetooth-device-list-item');
      assertEquals(2, devices.length);

      // Select a device.
      MockInteractions.tap(devices[0].$$('div'));
      Polymer.dom.flush();
      // Ensure the pairing dialog is shown.
      assertTrue(!!dialog.$$('#pairing'));
      // Ensure the device is connected to.
      expectEquals(1, self.bluetoothPrivateApi_.connectedDevices_.size);
      var deviceAddress =
          self.bluetoothPrivateApi_.connectedDevices_.keys().next().value;

      // Close the dialog.
      var close = dialog.$.dialog.getCloseButton();
      assertTrue(!!close);
      MockInteractions.tap(close);
      Polymer.dom.flush();
      expectFalse(dialog.$.dialog.opened);
      var response = self.bluetoothPrivateApi_.pairingResponses_[deviceAddress];
      assertTrue(!!response);
      expectEquals(chrome.bluetoothPrivate.PairingResponse.CANCEL,
                   response.response);
    });
  });

  // Run all registered tests.
  mocha.run();
});
