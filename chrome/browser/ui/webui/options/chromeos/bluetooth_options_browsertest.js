// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#if defined(OS_CHROMEOS)');

function BluetoothWebUITest() {}

BluetoothWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Start tests from the main-settings page.
   */
  browsePreload: 'chrome://settings-frame/',

  /**
   * @override
   */
  preLoad: function() {
    this.makeAndRegisterMockHandler([
      'bluetoothEnableChange',
      'updateBluetoothDevice',
      'findBluetoothDevices',
      'stopBluetoothDeviceDiscovery',
    ]);
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
  },

  /**
   * Selects a bluetooth device from the list with the matching address.
   * @param {!Element} listElement A list of Bluetooth devices.
   * @param {{address: string,
   *          connectable: boolean,
   *          connected: boolean,
   *          name: string,
   *          paired: boolean}} device Description of the device.
   */
  selectDevice: function(listElement, device) {
    listElement.selectedItem = device;
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

TEST_F('BluetoothWebUITest', 'testEnableBluetooth', function() {
  assertEquals(this.browsePreload, document.location.href);
  expectFalse($('enable-bluetooth').checked);
  expectTrue($('bluetooth-paired-devices-list').parentNode.hidden);

  this.mockHandler.expects(once()).bluetoothEnableChange([true]).will(
      callFunction(function() {
        options.BrowserOptions.setBluetoothState(true);
      }));
  $('enable-bluetooth').click();

  expectFalse($('bluetooth-paired-devices-list').parentNode.hidden);
});

TEST_F('BluetoothWebUITest', 'testAddDevices', function() {
  assertEquals(this.browsePreload, document.location.href);

  var pairedDeviceList = $('bluetooth-paired-devices-list');
  var unpairedDeviceList = $('bluetooth-unpaired-devices-list');

  var fakePairedDevice = {
    address: '00:11:22:33:44:55',
    connectable: true,
    connected: false,
    name: 'Fake device',
    paired: true
  };

  var fakeUnpairedDevice = {
    address: '28:CF:DA:00:00:00',
    connectable: true,
    connected: false,
    name: 'Apple Magic Mouse',
    paired: false
  };

  var fakeUnpairedDevice2 = {
    address: '28:37:37:00:00:00',
    connectable: true,
    connected: false,
    name: 'Apple Wireless Keyboard',
    paired: false
  };

  // Ensure data models for the paired and unpaired device lists are properly
  // updated.
  var index = pairedDeviceList.find(fakePairedDevice.address);
  expectEquals(undefined, index);
  options.BrowserOptions.addBluetoothDevice(fakePairedDevice);
  index = pairedDeviceList.find(fakePairedDevice.address);
  expectEquals(0, index);

  // Ensure the DOM contents of the list are properly updated. The default
  // layout of a list creates DOM elements only for visible elements in the
  // list, which is problematic if the list is hidden at the time of layout.
  // The Bluetooth device lists overcomes this problem by using fixed sized
  // elements in an auto-expanding list. This test ensures the problem stays
  // fixed.
  expectTrue(!!this.getElementForDevice(pairedDeviceList,
                                        fakePairedDevice.name));
  expectFalse(!!this.getElementForDevice(unpairedDeviceList,
                                         fakePairedDevice.name));

  options.BrowserOptions.addBluetoothDevice(fakeUnpairedDevice);
  index = unpairedDeviceList.find(fakeUnpairedDevice.address);
  expectEquals(0, index);
  expectFalse(!!this.getElementForDevice(pairedDeviceList,
                                         fakeUnpairedDevice.name));
  expectTrue(!!this.getElementForDevice(unpairedDeviceList,
                                        fakeUnpairedDevice.name));

  // Test adding a second device to a list.
  options.BrowserOptions.addBluetoothDevice(fakeUnpairedDevice2);
  index = unpairedDeviceList.find(fakeUnpairedDevice2.address);
  expectEquals(1, index);
  expectTrue(!!this.getElementForDevice(unpairedDeviceList,
                                        fakeUnpairedDevice2.name));

  // Test clicking on the 'Add a device' button.
  this.mockHandler.expects(once()).findBluetoothDevices();
  $('bluetooth-add-device').click();
  expectFalse($('bluetooth-options').hidden);
  expectTrue($('bluetooth-add-device-apply-button').disabled);
  expectFalse($('bluetooth-add-device-cancel-button').disabled);

  Mock4JS.verifyAllMocks();
  Mock4JS.clearMocksToVerify();

  // Test selecting an element and clicking on the connect button.
  this.mockHandler.expects(once()).stopBluetoothDeviceDiscovery();
  this.mockHandler.expects(once()).updateBluetoothDevice(
      [fakeUnpairedDevice2.address, 'connect']);
  this.selectDevice(unpairedDeviceList, fakeUnpairedDevice2);
  var connectButton = $('bluetooth-add-device-apply-button');
  expectFalse(connectButton.disabled);
  connectButton.click();
});

TEST_F('BluetoothWebUITest', 'testDevicePairing', function() {
  assertEquals(this.browsePreload, document.location.href);

  var pairedDeviceList = $('bluetooth-paired-devices-list');
  var unpairedDeviceList = $('bluetooth-unpaired-devices-list');

  var fakeDevice = {
    address: '00:24:BE:00:00:00',
    connectable: true,
    connected: false,
    name: 'Sony BT-00',
    paired: false
  };

  options.BrowserOptions.addBluetoothDevice(fakeDevice);
  var index = unpairedDeviceList.find(fakeDevice.address);
  expectEquals(0, index);
  expectTrue(!!this.getElementForDevice(unpairedDeviceList,
                                        fakeDevice.name));

  // Simulate process of pairing a device.
  fakeDevice.pairing = 'bluetoothEnterPinCode';
  options.BrowserOptions.addBluetoothDevice(fakeDevice);

  // Verify that the pairing dialog is displayed with the proper options.
  expectFalse($('bluetooth-pairing').hidden);
  expectTrue($('bluetooth-pairing-passkey-display').hidden);
  expectTrue($('bluetooth-pairing-passkey-entry').hidden);
  expectFalse($('bluetooth-pairing-pincode-entry').hidden);

  // Connect button should be visible but disabled until a pincode is entered.
  expectFalse($('bluetooth-pair-device-connect-button').hidden);
  expectFalse($('bluetooth-pair-device-cancel-button').hidden);
  expectTrue($('bluetooth-pair-device-connect-button').disabled);
  expectFalse($('bluetooth-pair-device-cancel-button').disabled);

  // Simulate process of entering a pincode.
  var pincode = '123456';

  this.mockHandler.expects(once()).updateBluetoothDevice(
      [fakeDevice.address, 'connect', pincode]).will(
      callFunction(function() {
        delete fakeDevice.pairing;
        fakeDevice.paired = true;
        options.BrowserOptions.addBluetoothDevice(fakeDevice);
      }));

  this.fakeInput($('bluetooth-pincode'), pincode);
  $('bluetooth-pair-device-connect-button').click();

  // Verify that the device is removed from the unparied list and added to the
  // paired device list.
  expectTrue(!!this.getElementForDevice(pairedDeviceList,
                                        fakeDevice.name));
  expectFalse(!!this.getElementForDevice(unpairedDeviceList,
                                         fakeDevice.name));
});

TEST_F('BluetoothWebUITest', 'testConnectionState', function() {
  assertEquals(this.browsePreload, document.location.href);

  var pairedDeviceList = $('bluetooth-paired-devices-list');
  var connectButton = $('bluetooth-reconnect-device');

  var fakeDevice = {
    address: '00:24:BE:00:00:00',
    connectable: true,
    connected: false,
    name: 'Sony BT-00',
    paired: true
  };

  options.BrowserOptions.addBluetoothDevice(fakeDevice);
  var element = this.getElementForDevice(pairedDeviceList,
                                         fakeDevice.name);
  assertTrue(!!element);
  expectFalse(!!element.getAttribute('connected'));
  expectTrue(connectButton.disabled);

  // Simulate connecting to a previously paired device.
  this.selectDevice(pairedDeviceList, fakeDevice);
  expectFalse(connectButton.disabled);
  this.mockHandler.expects(once()).updateBluetoothDevice(
      [fakeDevice.address, 'connect']).will(
      callFunction(function() {
        fakeDevice.connected = true;
        options.BrowserOptions.addBluetoothDevice(fakeDevice);
      }));
  connectButton.click();
  element = this.getElementForDevice(pairedDeviceList,
                                     fakeDevice.name);
  assertTrue(!!element);
  expectTrue(!!element.getAttribute('connected'));
  var button = element.querySelector('.row-delete-button');
  expectTrue(!!button);

  Mock4JS.verifyAllMocks();
  Mock4JS.clearMocksToVerify();

  // Test disconnecting.
  this.mockHandler.expects(once()).updateBluetoothDevice(
      [fakeDevice.address, 'disconnect']).will(
      callFunction(function() {
        fakeDevice.connected = false;
        options.BrowserOptions.addBluetoothDevice(fakeDevice);
      }));
  button.click();
  element = this.getElementForDevice(pairedDeviceList,
                                     fakeDevice.name);
  assertTrue(!!element);
  expectFalse(!!element.getAttribute('connected'));
  button = element.querySelector('.row-delete-button');
  expectTrue(!!button);

  Mock4JS.verifyAllMocks();
  Mock4JS.clearMocksToVerify();

  // Test forgetting  a disconnected device.
  this.mockHandler.expects(once()).updateBluetoothDevice(
      [fakeDevice.address, 'forget']).will(
      callFunction(function() {
        options.BrowserOptions.removeBluetoothDevice(fakeDevice.address);
      }));
  button.click();
  expectFalse(!!this.getElementForDevice(pairedDeviceList,
                                         fakeDevice.name));
});


TEST_F('BluetoothWebUITest', 'testMaliciousInput', function() {
  assertEquals(this.browsePreload, document.location.href);

  var unpairedDeviceList = $('bluetooth-unpaired-devices-list');
  var pairDeviceDialog = $('bluetooth-pairing');

  var maliciousStrings = [
      '<SCRIPT>alert(1)</SCRIPT>',
      '>\'>\\"><SCRIPT>alert(1)</SCRIPT>',
      '<IMG SRC=\\"javascript:alert(1)\\">',
      '<A HREF=\\"data:text/html;base64,' +
          'PHNjcmlwdD5hbGVydCgxKTwvc2NyaXB0Pgo=\\">...</A>',
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

  var fakeDevice = {
    address: '11:22:33:44:55:66',
    connectable: true,
    connected: false,
    name: 'fake',
    paired: false,
    pairing: 'bluetoothStartConnecting'
  };

  options.BrowserOptions.addBluetoothDevice(fakeDevice);

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

  // Determine the expected sizes.
  var unpairedDeviceListSize = nodeCount(unpairedDeviceList);
  var pairDeviceDialogSize = nodeCount(pairDeviceDialog);

  // Ensure that updating the device with a malicious name does not corrupt the
  // structure of the document.  Tests the unpaired device list and bluetooth
  // pairing dialog.
  for (var i = 0; i < maliciousStrings.length; i++) {
    fakeDevice.name = maliciousStrings[i];
    options.BrowserOptions.addBluetoothDevice(fakeDevice);
    assertEquals(unpairedDeviceListSize, nodeCount(unpairedDeviceList));
    var element = this.getElementForDevice(unpairedDeviceList,
                                           fakeDevice.name);
    assertTrue(!!element);
    var label = element.querySelector('.bluetooth-device-label');
    assertTrue(!!label);
    assertEquals(maliciousStrings[i], label.textContent);
    assertEquals(pairDeviceDialogSize, nodeCount(pairDeviceDialog));
  }

});

GEN('#endif');
