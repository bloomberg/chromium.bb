// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getFakePrefs() {
  return {
    ash: {
      user: {
        bluetooth: {
          adapter_enabled: {
            key: 'ash.user.bluetooth.adapter_enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          }
        }
      }
    }
  };
}

suite('Bluetooth', function() {
  let bluetoothPage = null;

  /** @type {Bluetooth} */
  let bluetoothApi_;

  /** @type {BluetoothPrivate} */
  let bluetoothPrivateApi_;

  /** @type {!chrome.bluetooth.Device} */
  let fakeUnpairedDevice1 = {
    address: '00:00:00:00:00:01',
    name: 'FakeUnpairedDevice1',
    paired: false,
    connected: false,
  };

  /** @type {!chrome.bluetooth.Device} */
  let fakeUnpairedDevice2 = {
    address: '00:00:00:00:00:02',
    name: 'FakeUnpairedDevice2',
    paired: false,
    connected: false,
  };

  /** @type {!chrome.bluetooth.Device} */
  let fakePairedDevice1 = {
    address: '10:00:00:00:00:01',
    name: 'FakePairedDevice1',
    paired: true,
    connected: true,
  };

  /** @type {!chrome.bluetooth.Device} */
  let fakePairedDevice2 = {
    address: '10:00:00:00:00:02',
    name: 'FakePairedDevice2',
    paired: true,
    connected: false,
  };

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
    bluetoothPage.prefs = getFakePrefs();
    assertTrue(!!bluetoothPage);

    bluetoothApi_.setDevicesForTest([]);
    document.body.appendChild(bluetoothPage);
    Polymer.dom.flush();
  });

  teardown(function() {
    bluetoothPage.remove();
  });

  test('MainPage', function() {
    assertFalse(bluetoothApi_.getAdapterStateForTest().powered);
    assertFalse(bluetoothPage.bluetoothToggleState_);
    // Test that tapping the single settings-box div enables bluetooth.
    const div = bluetoothPage.$$('div.settings-box');
    assertTrue(!!div);
    div.click();
    assertTrue(bluetoothPage.bluetoothToggleState_);
    assertTrue(bluetoothApi_.getAdapterStateForTest().powered);
  });

  suite('SubPage', function() {
    let subpage;

    function flushAsync() {
      Polymer.dom.flush();
      return new Promise(resolve => {
        bluetoothPage.async(resolve);
      });
    }

    setup(async function() {
      bluetoothApi_.setEnabled(true);
      Polymer.dom.flush();
      const div = bluetoothPage.$$('div.settings-box');
      div.click();

      await flushAsync();

      subpage = bluetoothPage.$$('settings-bluetooth-subpage');
      subpage.listUpdateFrequencyMs = 0;
      assertTrue(!!subpage);
      assertTrue(subpage.bluetoothToggleState);
      assertFalse(subpage.stateChangeInProgress);
      assertEquals(0, subpage.listUpdateFrequencyMs);
    });

    test('toggle', function() {
      assertTrue(subpage.bluetoothToggleState);

      const enableButton = subpage.$.enableBluetooth;
      assertTrue(!!enableButton);
      assertTrue(enableButton.checked);

      subpage.bluetoothToggleState = false;
      assertFalse(enableButton.checked);
      assertFalse(bluetoothApi_.getAdapterStateForTest().powered);
      assertFalse(bluetoothPage.bluetoothToggleState_);
    });

    // listUpdateFrequencyMs is set to 0 for tests, but we still need to wait
    // for the callback of setTimeout(0) to be processed in the message queue.
    // Add another setTimeout(0) to the end of message queue and wait for it to
    // complete ensures the previous callback has been executed.
    function waitForListUpdateTimeout() {
      return new Promise(function(resolve) {
        setTimeout(resolve, 0);
      });
    }

    test('pair device', async function() {
      bluetoothApi_.setDevicesForTest([
        fakeUnpairedDevice1, fakeUnpairedDevice2, fakePairedDevice1,
        fakePairedDevice2
      ]);
      await waitForListUpdateTimeout();

      Polymer.dom.flush();
      assertEquals(4, subpage.deviceList_.length);
      assertEquals(2, subpage.pairedDeviceList_.length);
      assertEquals(2, subpage.unpairedDeviceList_.length);

      const address = subpage.unpairedDeviceList_[0].address;
      await new Promise(
          resolve => bluetoothPrivateApi_.connect(address, resolve));

      Polymer.dom.flush();
      assertEquals(3, subpage.pairedDeviceList_.length);
      assertEquals(1, subpage.unpairedDeviceList_.length);
    });

    test('pair dialog', async function() {
      bluetoothApi_.setDevicesForTest([
        fakeUnpairedDevice1, fakeUnpairedDevice2, fakePairedDevice1,
        fakePairedDevice2
      ]);
      await waitForListUpdateTimeout();

      Polymer.dom.flush();
      const dialog = subpage.$.deviceDialog;
      assertTrue(!!dialog);
      assertFalse(dialog.$.dialog.open);

      // Simulate selecting an unpaired device; should show the pair dialog.
      subpage.connectDevice_(subpage.unpairedDeviceList_[0]);
      Polymer.dom.flush();
      assertTrue(dialog.$.dialog.open);
    });

    suite('Device List', function() {
      function deviceList() {
        return subpage.deviceList_;
      }

      function unpairedDeviceList() {
        return subpage.unpairedDeviceList_;
      }

      function pairedDeviceList() {
        return subpage.pairedDeviceList_;
      }

      let unpairedContainer;
      let unpairedDeviceIronList;

      let pairedContainer;
      let pairedDeviceIronList;

      setup(function() {
        unpairedContainer = subpage.$.unpairedContainer;
        assertTrue(!!unpairedContainer);
        assertTrue(unpairedContainer.hidden);
        unpairedDeviceIronList = subpage.$.unpairedDevices;
        assertTrue(!!unpairedDeviceIronList);

        pairedContainer = subpage.$.pairedContainer;
        assertTrue(!!pairedContainer);
        assertTrue(pairedContainer.hidden);
        pairedDeviceIronList = subpage.$.pairedDevices;
        assertTrue(!!pairedDeviceIronList);
      });

      test('Unpaired devices: devices added', async function() {
        bluetoothApi_.setDevicesForTest(
            [fakeUnpairedDevice1, fakeUnpairedDevice2]);
        await waitForListUpdateTimeout();
        Polymer.dom.flush();

        assertEquals(2, deviceList().length);
        assertEquals(2, unpairedDeviceList().length);
        assertEquals(0, pairedDeviceList().length);
        assertTrue(subpage.$.noUnpairedDevices.hidden);
        assertFalse(subpage.$.noPairedDevices.hidden);

        unpairedDeviceIronList.notifyResize();
        Polymer.dom.flush();

        const devices = unpairedDeviceIronList.querySelectorAll(
            'bluetooth-device-list-item');
        assertEquals(2, devices.length);
        assertFalse(devices[0].device.paired);
        assertFalse(devices[1].device.paired);
      });

      test('Paired devices: devices added', async function() {
        bluetoothApi_.setDevicesForTest([fakePairedDevice1, fakePairedDevice2]);
        await waitForListUpdateTimeout();
        Polymer.dom.flush();

        assertEquals(2, deviceList().length);
        assertEquals(0, unpairedDeviceList().length);
        assertEquals(2, pairedDeviceList().length);
        assertFalse(subpage.$.noUnpairedDevices.hidden);
        assertTrue(subpage.$.noPairedDevices.hidden);

        pairedDeviceIronList.notifyResize();
        Polymer.dom.flush();

        const devices =
            pairedDeviceIronList.querySelectorAll('bluetooth-device-list-item');
        assertEquals(2, devices.length);
        assertTrue(devices[0].device.paired);
        assertTrue(devices[0].device.connected);
        assertTrue(devices[1].device.paired);
        assertFalse(devices[1].device.connected);
      });

      test('Unpaired and paired devices: devices added', async function() {
        bluetoothApi_.setDevicesForTest([
          fakeUnpairedDevice1, fakeUnpairedDevice2, fakePairedDevice1,
          fakePairedDevice2
        ]);

        await waitForListUpdateTimeout();
        Polymer.dom.flush();

        assertEquals(4, deviceList().length);
        assertEquals(2, unpairedDeviceList().length);
        assertEquals(2, pairedDeviceList().length);
        assertTrue(subpage.$.noUnpairedDevices.hidden);
        assertTrue(subpage.$.noPairedDevices.hidden);

        pairedDeviceIronList.notifyResize();
        unpairedDeviceIronList.notifyResize();
        Polymer.dom.flush();

        const unpairedDevices = unpairedDeviceIronList.querySelectorAll(
            'bluetooth-device-list-item');
        assertEquals(2, unpairedDevices.length);
        assertFalse(unpairedDevices[0].device.paired);
        assertFalse(unpairedDevices[1].device.paired);

        const pairedDevices =
            pairedDeviceIronList.querySelectorAll('bluetooth-device-list-item');
        assertEquals(2, pairedDevices.length);
        assertTrue(pairedDevices[0].device.paired);
        assertTrue(pairedDevices[0].device.connected);
        assertTrue(pairedDevices[1].device.paired);
        assertFalse(pairedDevices[1].device.connected);
      });
    });
  });
});
