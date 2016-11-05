// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#if defined(OS_CHROMEOS)');

GEN_INCLUDE(['../options_browsertest_base.js']);

function BluetoothWebUITestAsync() {}

BluetoothWebUITestAsync.prototype = {
  __proto__: OptionsBrowsertestBase.prototype,

  /** @override */
  isAsync: true,

  /**
   * Start tests from the main-settings page.
   */
  browsePreload: 'chrome://settings-frame/',

  // These entries match the fake entries in FakeBluetoothDeviceClient.
  fakePairedDevice: {
    address: '00:11:22:33:44:55',
    connectable: true,
    connected: false,
    name: 'Fake Device (name)',
    paired: true
  },

  fakePairedDevice2: {
    address: '20:7D:74:00:00:04',
    connectable: false,
    connected: false,
    name: 'Paired Unconnectable Device (name)',
    paired: true
  },

  fakeUnpairedDevice: {
    address: '28:CF:DA:00:00:00',
    connectable: true,
    connected: false,
    name: 'Bluetooth 2.0 Mouse',
    paired: false
  },

  fakeUnpairedDevice2: {
    address: '00:24:BE:00:00:00',
    connectable: true,
    connected: false,
    name: 'PIN Device',
    paired: false
  },

  /** @override */
  setUp: function() {
    OptionsBrowsertestBase.prototype.setUp.call(this);

    var unsupportedAriaAttributeSelectors = [
      '#bluetooth-paired-devices-list',
      '#bluetooth-unpaired-devices-list',
    ];

    // Enable when failure is resolved.
    // AX_ARIA_10: http://crbug.com/570564
    this.accessibilityAuditConfig.ignoreSelectors(
        'unsupportedAriaAttribute',
        unsupportedAriaAttributeSelectors);
  },

  /**
   * Retrieves the list item associated with a Bluetooth device.
   * @param {!Element} listElement Element containing a list of devices.
   * @param {string} deviceName The name of the device.
   * @return {Element|undefined} List item matching the device name.
   */
  getElementForDevice: function(listElement, deviceName) {
    var items = listElement.querySelectorAll('.bluetooth-device');
    for (var i = 0; i < items.length; i++) {
      var candidate = items[i];
      var name = candidate.data.name;
      if (name == deviceName)
        return candidate;
    }
    return undefined;
  },

  /**
   * Selects a bluetooth device from the list with the matching address.
   * @param {!Element} listElement A list of Bluetooth devices.
   * @param {string} address Device address.
   */
  selectDevice: function(listElement, address) {
    listElement.setSelectedDevice_(address);
    cr.dispatchSimpleEvent(listElement, 'change');
  },

  /**
   * Fake input of a pincode or passkey.
   * @param {!Element} element Text input field.
   * @param {string} text New value for the input field.
   */
  fakeInput: function(element, text) {
    element.value = text;
    cr.dispatchSimpleEvent(element, 'input');
  },
};

TEST_F('BluetoothWebUITestAsync', 'testEnableBluetooth', function() {
  assertEquals(this.browsePreload, document.location.href);
  expectFalse($('enable-bluetooth').checked);
  expectTrue($('bluetooth-paired-devices-list').parentNode.hidden);

  $('enable-bluetooth').click();

  // The UI may not be updated until all callbacks have been handled, so
  // send a new request that will get processed after any currently pending
  // callbacks.
  chrome.bluetooth.getAdapterState(function(state) {
    expectTrue(state.powered);
    expectFalse($('bluetooth-paired-devices-list').parentNode.hidden);
    testDone();
  }.bind(this));
});

// TODO(crbug.com/603499) Test is flaky.
TEST_F('BluetoothWebUITestAsync', 'DISABLED_testAddDevice', function() {
  assertEquals(this.browsePreload, document.location.href);

  // Enable bluetooth.
  $('enable-bluetooth').click();

  // Wait for the UI to process any pending messages.
  window.setTimeout(function() {
    // Wait for fake bluetooth impl to send any updates.
    chrome.bluetooth.getAdapterState(function(state) {
      var pairedDeviceList = $('bluetooth-paired-devices-list');
      var unpairedDeviceList = $('bluetooth-unpaired-devices-list');

      // Verify that devices are in the correct list.
      var index = pairedDeviceList.find(this.fakePairedDevice.address);
      expectEquals(1, index);
      index = pairedDeviceList.find(this.fakePairedDevice2.address);
      expectEquals(0, index);
      index = pairedDeviceList.find(this.fakeUnpairedDevice.address);
      expectEquals(undefined, index);
      expectTrue(!!this.getElementForDevice(pairedDeviceList,
                                            this.fakePairedDevice.name));
      expectFalse(!!this.getElementForDevice(unpairedDeviceList,
                                             this.fakePairedDevice.name));

      // Test clicking on the 'Add a device' button. This should send a
      // startDiscovering request.
      $('bluetooth-add-device').click();
      expectFalse($('bluetooth-options').hidden);

      // Wait for fake bluetooth impl to send any updates.
      chrome.bluetooth.getAdapterState(function(state) {
        expectTrue(state.discovering);
        expectFalse(unpairedDeviceList.parentNode.hidden);

        index = unpairedDeviceList.find(this.fakeUnpairedDevice.address);
        expectEquals(0, index);

        var connectButton = $('bluetooth-add-device-apply-button');
        expectTrue(connectButton.disabled);
        expectFalse($('bluetooth-add-device-cancel-button').disabled);

        // Test selecting an element and clicking on the connect button.
        this.selectDevice(unpairedDeviceList, this.fakeUnpairedDevice.address);
        expectFalse(connectButton.disabled);
        connectButton.click();

        // Wait for fake bluetooth impl to send any updates.
        chrome.bluetooth.getAdapterState(function(state) {
          // Verify that the pairing UI is shown.
          expectFalse($('bluetooth-pairing').hidden);
          testDone();
        }.bind(this));
      }.bind(this));
    }.bind(this));
  }.bind(this));
});

TEST_F('BluetoothWebUITestAsync', 'testDevicePairing', function() {
  assertEquals(this.browsePreload, document.location.href);

  // Enable bluetooth.
  $('enable-bluetooth').click();

  // Wait for the UI to process any pending messages.
  window.setTimeout(function() {
    // Wait for fake bluetooth impl to send any updates.
    chrome.bluetooth.getAdapterState(function(state) {
      var pairedDeviceList = $('bluetooth-paired-devices-list');
      var unpairedDeviceList = $('bluetooth-unpaired-devices-list');

      $('bluetooth-add-device').click();

      // Wait for fake bluetooth impl to send any updates.
      chrome.bluetooth.getAdapterState(function(state) {
        expectFalse(unpairedDeviceList.parentNode.hidden);

        // Test selecting an element and clicking on the connect button.
        var index = unpairedDeviceList.find(this.fakeUnpairedDevice2.address);
        expectNotEquals(undefined, index);
        this.selectDevice(unpairedDeviceList, this.fakeUnpairedDevice2.address);
        var connectButton = $('bluetooth-add-device-apply-button');
        expectFalse(connectButton.disabled);
        connectButton.click();

        // Wait for fake bluetooth impl to send any updates.
        chrome.bluetooth.getAdapterState(function(state) {
          // Verify that the pairing UI is shown.
          expectFalse($('bluetooth-pairing').hidden);
          expectTrue($('bluetooth-pairing-passkey-display').hidden);
          expectTrue($('bluetooth-pairing-passkey-entry').hidden);
          expectFalse($('bluetooth-pairing-pincode-entry').hidden);

          var pincode = '123456';
          this.fakeInput($('bluetooth-pincode'), pincode);
          $('bluetooth-pair-device-connect-button').click();

          // Wait for fake bluetooth impl to send any updates.
          chrome.bluetooth.getAdapterState(function(state) {
            expectTrue($('bluetooth-pairing-pincode-entry').hidden);
            testDone();
          }.bind(this));
        }.bind(this));
      }.bind(this));
    }.bind(this));
  }.bind(this));
});

// TODO(crbug.com/608126) Test is flaky.
TEST_F('BluetoothWebUITestAsync', 'DISABLED_testConnect', function() {
  assertEquals(this.browsePreload, document.location.href);

  // Enable bluetooth.
  $('enable-bluetooth').click();

  // Wait for the UI to process any pending messages.
  window.setTimeout(function() {
    // Wait for fake bluetooth impl to send any updates.
    chrome.bluetooth.getAdapterState(function(state) {
      var pairedDeviceList = $('bluetooth-paired-devices-list');
      var element = this.getElementForDevice(
          pairedDeviceList, this.fakePairedDevice.name);
      assertTrue(!!element, this.fakePairedDevice.name);
      expectFalse(!!element.getAttribute('connected'));

      var connectButton = $('bluetooth-reconnect-device');
      expectTrue(connectButton.disabled);

      // Simulate connecting to a previously paired device.
      this.selectDevice(pairedDeviceList, this.fakePairedDevice.address);
      expectFalse(connectButton.disabled);
      connectButton.click();

      // Call bluetooth.getAdapterState to ensure that all state has been
      // updated.
      chrome.bluetooth.getAdapterState(function(state) {
        element = this.getElementForDevice(
            pairedDeviceList, this.fakePairedDevice.name);
        expectTrue(!!element.getAttribute('connected'));
        var deleteButton = element.querySelector('.row-delete-button');
        expectTrue(!!deleteButton);
        testDone();
      }.bind(this));
    }.bind(this));
  }.bind(this));
});

TEST_F('BluetoothWebUITestAsync', 'testDisconnect', function() {
  assertEquals(this.browsePreload, document.location.href);

  // Enable bluetooth.
  $('enable-bluetooth').click();

  // Wait for the UI to process any pending messages.
  window.setTimeout(function() {
    // Wait for fake bluetooth impl to send any updates.
    chrome.bluetooth.getAdapterState(function(state) {
      var pairedDeviceList = $('bluetooth-paired-devices-list');

      // First connect to the device so that the fake implementation state is
      // connected.
      chrome.bluetoothPrivate.connect(
          this.fakePairedDevice.address, function(result) {
            assertEquals(
                chrome.bluetoothPrivate.ConnectResultType.SUCCESS, result);

            var element = this.getElementForDevice(
                pairedDeviceList, this.fakePairedDevice.name);
            assertTrue(!!element, this.fakePairedDevice.name);
            expectTrue(!!element.getAttribute('connected'));

            // Simulate disconnecting from a connected device.
            var button = element.querySelector('.row-delete-button');
            button.click();

            // Wait for fake bluetooth impl to send any updates.
            chrome.bluetooth.getAdapterState(function(state) {
              element = this.getElementForDevice(
                  pairedDeviceList, this.fakePairedDevice.name);
              expectFalse(!!element.getAttribute('connected'));
              button = element.querySelector('.row-delete-button');
              expectTrue(!!button);
              testDone();
            }.bind(this));
          }.bind(this));
    }.bind(this));
  }.bind(this));
});

// TODO(crbug.com/605090): Disabled because of flakiness.
TEST_F('BluetoothWebUITestAsync', 'DISABLED_testForget', function() {
  assertEquals(this.browsePreload, document.location.href);

  // Enable bluetooth.
  $('enable-bluetooth').click();

  // Wait for the UI to process any pending messages.
  window.setTimeout(function() {
    // Wait for fake bluetooth impl to send any updates.
    chrome.bluetooth.getAdapterState(function(state) {
      var pairedDeviceList = $('bluetooth-paired-devices-list');

      var element = this.getElementForDevice(pairedDeviceList,
                                             this.fakePairedDevice.name);
      var button = element.querySelector('.row-delete-button');
      button.click();

      // Wait for fake bluetooth impl to send any updates.
      chrome.bluetooth.getAdapterState(function(state) {
        expectFalse(!!this.getElementForDevice(pairedDeviceList,
                                               this.fakePairedDevice.name));
        testDone();
      }.bind(this));
    }.bind(this));
  }.bind(this));
});


TEST_F('BluetoothWebUITestAsync', 'testMaliciousInput', function() {
  assertEquals(this.browsePreload, document.location.href);

  var maliciousStrings = [
    '<SCRIPT>alert(1)</SCRIPT>',
    '>\'>\\"><SCRIPT>alert(1)</SCRIPT>',
    '<IMG SRC=\\"javascript:alert(1)\\">',
    '<A HREF=\\"data:text/html;base64,' +
        'PHNjcmlwdD5hbGVydCgxKTwvc2NyaXB0Pgo=\\">..</A>',
    '<div>',
    '<textarea>',
    '<style>',
    '[0xC0][0xBC]SCRIPT[0xC0][0xBE]alert(1)[0xC0][0xBC]/SCRIPT[0xC0][0xBE]',
    '+ADw-SCRIPT+AD4-alert(1)+ADw-/SCRIPT+AD4-',
    '&#<script>alert(1)</script>;',
    '<!-- Hello -- world > <SCRIPT>alert(1)</SCRIPT> -->',
    '<!<!-- Hello world > <SCRIPT>alert(1)</SCRIPT> -->',
    '\x3CSCRIPT\x3Ealert(1)\x3C/SCRIPT\x3E',
    '<IMG SRC=\\"j[0x00]avascript:alert(1)\\">',
    '<BASE HREF=\\"javascript:1;/**/\\"><IMG SRC=\\"alert(1)\\">',
    'javascript:alert(1);',
    ' xss_injection=\\"\\" ',
    '\\" xss_injection=\\"',
    '\' xss_injection=\'',
    '<!--',
    '\'',
    '\\"'
  ];

  var fakeEvent = {
    device: {
      address: '28:CF:DA:00:00:00',
      connectable: true,
      connected: false,
      name: 'Bluetooth 2.0 Mouse',
      paired: false
    },
    pairing: 'bluetoothStartConnecting'
  };

  var nodeCount = function(node) {
    if (node.getAttribute)
      assertFalse(!!node.getAttribute('xss_injection'));
    var length = node.childNodes.length;
    var tally = length;
    for (var i = 0; i < length; i++) {
      tally += nodeCount(node.childNodes[i]);
    }
    return tally;
  };

  // Enable bluetooth.
  $('enable-bluetooth').click();

  // Wait for the UI to process any pending messages.
  window.setTimeout(function() {
    // Wait for fake bluetooth impl to send any updates.
    chrome.bluetooth.getAdapterState(function(state) {
      var unpairedDeviceList = $('bluetooth-unpaired-devices-list');
      var pairDeviceDialog = $('bluetooth-pairing');

      // Show the pairing dialog.
      $('bluetooth-add-device').click();
      BluetoothPairing.showDialog(fakeEvent);

      // Wait for fake bluetooth impl to send any updates.
      chrome.bluetooth.getAdapterState(function(state) {
        expectFalse(unpairedDeviceList.parentNode.hidden);

        // Determine the expected sizes.
        var unpairedDeviceListSize = nodeCount(unpairedDeviceList);
        var pairDeviceDialogSize = nodeCount(pairDeviceDialog);

        // Ensure that updating the device with a malicious name does not
        // corrupt the structure of the document.  Tests the unpaired device
        // list and bluetooth pairing dialog.
        for (var i = 0; i < maliciousStrings.length; i++) {
          fakeEvent.device.name = maliciousStrings[i];
          BluetoothPairing.showDialog(fakeEvent);
          assertEquals(unpairedDeviceListSize, nodeCount(unpairedDeviceList));
          var element = this.getElementForDevice(
              unpairedDeviceList, fakeEvent.device.name);
          assertTrue(!!element, fakeEvent.device.name);
          var label = element.querySelector('.bluetooth-device-label');
          assertTrue(!!label);
          assertEquals(maliciousStrings[i], label.textContent);
          assertEquals(pairDeviceDialogSize, nodeCount(pairDeviceDialog));
        }

        testDone();
      }.bind(this));
    }.bind(this));
  }.bind(this));
});

GEN('#endif');
