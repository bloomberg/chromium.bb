// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('Bluetooth', function() {
  var bluetoothPage = null;

  /** @type {Bluetooth} */
  var bluetoothApi_;

  /** @type {BluetoothPrivate} */
  var bluetoothPrivateApi_;

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

  suiteSetup(function() {
    loadTimeData.overrideValues({
      bluetoothEnabled: 'bluetoothEnabled',
      bluetoothDisabled: 'bluetoothDisabled',
      bluetoothOn: 'bluetoothOn',
      bluetoothOff: 'bluetoothOff',
      bluetoothConnected: 'bluetoothConnected',
      bluetoothDisconnect: 'bluetoothDisconnect',
      bluetoothPair: 'bluetoothPair',
      bluetoothStartConnecting: 'bluetoothStartConnecting',
    });

    bluetoothApi_ = new settings.FakeBluetooth();
    bluetoothPrivateApi_ = new settings.FakeBluetoothPrivate(bluetoothApi_);

    // Set globals to override Settings Bluetooth Page apis.
    bluetoothApis.bluetoothApiForTest = bluetoothApi_;
    bluetoothApis.bluetoothPrivateApiForTest = bluetoothPrivateApi_;

    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  setup(function() {
    PolymerTest.clearBody();
    bluetoothPage = document.createElement('settings-bluetooth-page');
    assertTrue(!!bluetoothPage);

    document.body.appendChild(bluetoothPage);
    Polymer.dom.flush();
  });

  teardown(function() {
    bluetoothPage.remove();
  });

  test('MainPage', function() {
    assertFalse(bluetoothApi_.adapterState.powered);
    assertFalse(bluetoothPage.bluetoothEnabled_);
    // Test that tapping the single settings-box div enables bluetooth.
    var div = bluetoothPage.$$('div.settings-box');
    assertTrue(!!div);
    MockInteractions.tap(div);
    assertTrue(bluetoothPage.bluetoothEnabled_);
    assertTrue(bluetoothApi_.adapterState.powered);
  });

  suite('SubPage', function() {
    var subpage;

    setup(function() {
      bluetoothPage.bluetoothEnabled_ = true;
      var div = bluetoothPage.$$('div.settings-box');
      MockInteractions.tap(div);
      subpage = bluetoothPage.$$('settings-bluetooth-subpage');
      assertTrue(!!subpage);
    });

    test('toggle', function() {
      assertTrue(subpage.bluetoothEnabled);

      var enableButton = subpage.$.enableBluetooth;
      assertTrue(!!enableButton);
      assertTrue(enableButton.checked);

      subpage.bluetoothEnabled = false;
      assertFalse(enableButton.checked);
      assertFalse(bluetoothApi_.adapterState.powered);;
      assertFalse(bluetoothPage.bluetoothEnabled_);
    });

    test('device list', function() {
      var deviceList = subpage.$.container;
      assertTrue(!!deviceList);
      assertTrue(deviceList.hidden);
      assertFalse(subpage.$.noDevices.hidden);

      bluetoothApi_.setDevicesForTest(fakeDevices_);
      Polymer.dom.flush();
      assertEquals(4, subpage.deviceList_.length);
      assertTrue(subpage.$.noDevices.hidden);

      var devicesIronList = subpage.$$('#container > iron-list');
      assertTrue(!!devicesIronList);
      devicesIronList.notifyResize();
      Polymer.dom.flush();
      var devices = deviceList.querySelectorAll('bluetooth-device-list-item');
      assertEquals(2, devices.length);
      assertTrue(devices[0].device.connected);
      assertFalse(devices[1].device.connected);
      assertTrue(devices[1].device.connecting);
    });

    test('device dialog', function() {
      // Tap the 'add device' button.
      MockInteractions.tap(subpage.$.pairButton);
      Polymer.dom.flush();

      // Ensure the dialog appears.
      var dialog = subpage.$.deviceDialog;
      assertTrue(!!dialog);
      assertTrue(dialog.$.dialog.open);
      assertEquals(dialog.deviceList.length, 4);

      // Ensure the dialog has the expected devices.
      var devicesIronList = dialog.$$('#dialogDeviceList iron-list');
      assertTrue(!!devicesIronList);
      devicesIronList.notifyResize();
      Polymer.dom.flush();
      var devices =
          devicesIronList.querySelectorAll('bluetooth-device-list-item');
      assertEquals(devices.length, 2);

      // Select a device.
      MockInteractions.tap(devices[0].$$('div'));
      Polymer.dom.flush();
      // Ensure the pairing dialog is shown.
      assertTrue(!!dialog.$$('#pairing'));

      // Ensure the device wass connected to.
      expectEquals(1, bluetoothPrivateApi_.connectedDevices_.size);

      // Close the dialog.
      dialog.close();
      Polymer.dom.flush();
      assertFalse(dialog.$.dialog.open);
    });
  });
});
