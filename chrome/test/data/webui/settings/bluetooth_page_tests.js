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
    },
    {
      address: '00:00:00:00:00:01',
      name: 'FakeUnpairedDevice1',
      paired: false,
    },
    {
      address: '00:00:00:00:00:02',
      name: 'FakeUnpairedDevice2',
      paired: false,
    },
  ];

  suiteSetup(function() {
    loadTimeData.overrideValues({
      deviceOff: 'deviceOff',
      deviceOn: 'deviceOn',
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

    bluetoothApi_.setDevicesForTest([]);
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

    test('paired device list', function() {
      var pairedContainer = subpage.$.pairedContainer;
      assertTrue(!!pairedContainer);
      assertTrue(pairedContainer.hidden);
      assertFalse(subpage.$.noPairedDevices.hidden);

      bluetoothApi_.setDevicesForTest(fakeDevices_);
      Polymer.dom.flush();
      assertEquals(4, subpage.deviceList_.length);
      assertEquals(2, subpage.pairedDeviceList_.length);
      assertTrue(subpage.$.noPairedDevices.hidden);

      var ironList = subpage.$.pairedDevices;
      assertTrue(!!ironList);
      ironList.notifyResize();
      Polymer.dom.flush();
      var devices = ironList.querySelectorAll('bluetooth-device-list-item');
      assertEquals(2, devices.length);
      assertTrue(devices[0].device.connected);
      assertFalse(devices[1].device.connected);
    });

    test('unpaired device list', function() {
      var unpairedContainer = subpage.$.unpairedContainer;
      assertTrue(!!unpairedContainer);
      assertTrue(unpairedContainer.hidden);
      assertFalse(subpage.$.noUnpairedDevices.hidden);

      bluetoothApi_.setDevicesForTest(fakeDevices_);
      Polymer.dom.flush();
      assertEquals(4, subpage.deviceList_.length);
      assertEquals(2, subpage.unpairedDeviceList_.length);
      assertTrue(subpage.$.noUnpairedDevices.hidden);

      var ironList = subpage.$.unpairedDevices;
      assertTrue(!!ironList);
      ironList.notifyResize();
      Polymer.dom.flush();
      var devices = ironList.querySelectorAll('bluetooth-device-list-item');
      assertEquals(2, devices.length);
      assertFalse(devices[0].device.paired);
      assertFalse(devices[1].device.paired);
    });

    test('pair device', function(done) {
      bluetoothApi_.setDevicesForTest(fakeDevices_);
      Polymer.dom.flush();
      assertEquals(4, subpage.deviceList_.length);
      assertEquals(2, subpage.pairedDeviceList_.length);
      assertEquals(2, subpage.unpairedDeviceList_.length);

      var address = subpage.unpairedDeviceList_[0].address;
      bluetoothPrivateApi_.connect(address, function() {
        Polymer.dom.flush();
        assertEquals(3, subpage.pairedDeviceList_.length);
        assertEquals(1, subpage.unpairedDeviceList_.length);
        done();
      });
    });

    test('pair dialog', function() {
      bluetoothApi_.setDevicesForTest(fakeDevices_);
      Polymer.dom.flush();
      var dialog = subpage.$.deviceDialog;
      assertTrue(!!dialog);
      assertFalse(dialog.$.dialog.open);

      // Simulate selecting an unpaired device; should show the pair dialog.
      subpage.connectDevice_(subpage.unpairedDeviceList_[0]);
      Polymer.dom.flush();
      assertTrue(dialog.$.dialog.open);
    });
  });
});
