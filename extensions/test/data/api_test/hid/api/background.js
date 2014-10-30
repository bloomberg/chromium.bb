// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var kInvalidDeviceId = -1;
var kInvalidConnectionId = -1;

function getDevice(wantReportIds, callback) {
  chrome.hid.getDevices({}, function (devices) {
    chrome.test.assertNoLastError();
    for (var device of devices) {
      chrome.test.assertTrue(device.collections.length > 0);
      var foundReportId = false;
      for (var collection of device.collections) {
        if (collection.reportIds.length > 0) {
          foundReportId = true;
        }
      }
      if (wantReportIds == foundReportId) {
        callback(device);
        return;
      }
    }
    chrome.test.fail("No appropriate device found.");
  });
}

function openDevice(wantReportIds, callback) {
  getDevice(wantReportIds, function (device) {
    chrome.hid.connect(device.deviceId, function (connection) {
      chrome.test.assertNoLastError();
      callback(connection.connectionId);
    });
  });
}

function openDeviceWithReportId(callback) {
  return openDevice(true, callback);
}

function openDeviceWithoutReportId(callback) {
  return openDevice(false, callback);
}

function arrayBufferToString(buffer) {
  return String.fromCharCode.apply(null, new Uint8Array(buffer));
}

function stringToArrayBuffer(string) {
  var buffer = new ArrayBuffer(string.length);
  var view = new Uint8Array(buffer);
  for (var i = 0; i < string.length; i++) {
    view[i] = string.charCodeAt(i);
  }
  return buffer;
}

function testGetDevicesWithNoOptions() {
  chrome.hid.getDevices({}, function (devices) {
    chrome.test.assertNoLastError();
    chrome.test.assertEq(2, devices.length, "Expected two enumerated devices.");
    chrome.test.succeed("Device enumeration successful.");
  });
};

function testGetDevicesWithLegacyVidAndPid() {
  chrome.hid.getDevices({
      'vendorId': 0x18D1,
      'productId': 0x58F0
    }, function (devices) {
    chrome.test.assertNoLastError();
    chrome.test.assertEq(2, devices.length, "Expected two enumerated devices.");
    chrome.test.succeed("Device enumeration successful.");
  });
};

function testGetDevicesWithNoFilters() {
  chrome.hid.getDevices({ 'filters': [] }, function (devices) {
    chrome.test.assertNoLastError();
    chrome.test.assertEq(2, devices.length, "Expected two enumerated devices.");
    chrome.test.succeed("Device enumeration successful.");
  });
};

function testGetDevicesWithVidPidFilter() {
  chrome.hid.getDevices({ 'filters': [
      { 'vendorId': 0x18D1, 'productId': 0x58F0}
    ] }, function (devices) {
    chrome.test.assertNoLastError();
    chrome.test.assertEq(2, devices.length, "Expected two enumerated devices.");
    chrome.test.succeed("Device enumeration successful.");
  });
};

function testGetDevicesWithUsageFilter() {
  chrome.hid.getDevices({ 'filters': [
      { 'usagePage': 0xFF00 }  /* vendor-specified usage page */
    ] }, function (devices) {
    chrome.test.assertNoLastError();
    chrome.test.assertEq(1, devices.length, "Expected one enumerated device.");
    var device = devices[0];
    chrome.test.assertEq(1, device.collections.length,
                         "Expected one collection.");
    var collection = device.collections[0];
    chrome.test.assertEq(0xFF00, collection.usagePage);
    chrome.test.succeed("Device enumeration successful.");
  });
}

function testGetDevicesWithUnauthorizedDevice() {
  chrome.hid.getDevices({ 'filters': [
      { 'vendorId': 0x18D1, 'productId': 0x58F1}
    ] }, function (devices) {
    chrome.test.assertNoLastError();
    chrome.test.assertEq(0, devices.length, "Expected no enumerated devices.");
    chrome.test.succeed("Device enumeration successful.");
  });
};

function testConnectWithInvalidDeviceId() {
  chrome.hid.connect(kInvalidDeviceId, function (connection) {
    chrome.test.assertLastError("Invalid HID device ID.");
    chrome.test.succeed("Rejected invalid device ID.");
  });
};

function testConnectAndDisconnect() {
  chrome.hid.getDevices({ 'filters': [
      { 'vendorId': 0x18D1, 'productId': 0x58F0 }
    ] }, function (devices) {
    chrome.test.assertNoLastError();
    chrome.test.assertTrue(devices.length >= 1, "Expected connectable device.");
    chrome.hid.connect(devices[0].deviceId, function (connection) {
      chrome.test.assertNoLastError();
      chrome.hid.disconnect(connection.connectionId, function () {
        chrome.test.assertNoLastError();
        chrome.test.succeed("Opened and closed device.");
      });
    });
  });
};

function testDisconnectWithInvalidConnectionId() {
  chrome.hid.disconnect(kInvalidConnectionId, function () {
    chrome.test.assertLastError("Connection not established.");
    chrome.test.succeed("Rejected invalid connection ID.");
  });
}

function testReceiveWithInvalidConnectionId() {
  chrome.hid.receive(kInvalidConnectionId, function (reportId, data) {
    chrome.test.assertLastError("Connection not established.");
    chrome.test.succeed("Rejected invalid connection ID.");
  });
}

function testReceiveWithReportId() {
  openDeviceWithReportId(function (connection) {
    chrome.hid.receive(connection, function (reportId, data) {
      chrome.test.assertEq(1, reportId, "Expected report_id == 1.");
      var expected = "This is a HID input report.";
      chrome.test.assertEq(expected, arrayBufferToString(data));
      chrome.test.succeed("Receive successful.");
    });
  });
}

function testReceiveWithoutReportId() {
  openDeviceWithoutReportId(function (connection) {
    chrome.hid.receive(connection, function (reportId, data) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(0, reportId, "Expected report_id == 0.");
      var expected = "This is a HID input report.";
      chrome.test.assertEq(expected, arrayBufferToString(data));
      chrome.test.succeed("Receive successful.");
    });
  });
}

function testSendWithInvalidConnectionId() {
  var buffer = new ArrayBuffer();
  chrome.hid.send(kInvalidConnectionId, 0, buffer, function () {
    chrome.test.assertLastError("Connection not established.");
    chrome.test.succeed("Rejected invalid connection ID.");
  });
}

function testSendWithReportId() {
  openDeviceWithReportId(function (connection) {
    var buffer = stringToArrayBuffer("This is a HID output report.");
    chrome.hid.send(connection, 1, buffer, function () {
      chrome.test.assertNoLastError();
      chrome.hid.disconnect(connection);
      chrome.test.succeed("Send successful.");
    });
  });
}

function testSendWithoutReportId() {
  openDeviceWithoutReportId(function (connection) {
    var buffer = stringToArrayBuffer("This is a HID output report.");
    chrome.hid.send(connection, 0, buffer, function () {
      chrome.test.assertNoLastError();
      chrome.hid.disconnect(connection);
      chrome.test.succeed("Send successful.");
    });
  });
}

function testSendWithInvalidReportId() {
  openDeviceWithReportId(function (connection) {
    var buffer = stringToArrayBuffer("This is a HID output report.");
    chrome.hid.send(connection, 0, buffer, function () {
      chrome.test.assertLastError("Transfer failed.");
      chrome.hid.disconnect(connection);
      chrome.test.succeed("Caught invalid report ID.");
    });
  });
}

function testSendWithUnexpectedReportId() {
  openDeviceWithoutReportId(function (connection) {
    var buffer = stringToArrayBuffer("This is a HID output report.");
    chrome.hid.send(connection, 1, buffer, function () {
      chrome.test.assertLastError("Transfer failed.");
      chrome.hid.disconnect(connection);
      chrome.test.succeed("Caught unexpected report ID.");
    });
  });
}

function testReceiveFeatureReportWithInvalidConnectionId() {
  chrome.hid.receiveFeatureReport(kInvalidConnectionId, 0, function (data) {
    chrome.test.assertLastError("Connection not established.");
    chrome.test.succeed("Rejected invalid connection ID.");
  });
}

function testReceiveFeatureReportWithReportId() {
  openDeviceWithReportId(function (connection) {
    chrome.hid.receiveFeatureReport(connection, 1, function (data) {
      chrome.test.assertNoLastError();
      var expected = "\1This is a HID feature report.";
      chrome.test.assertEq(expected, arrayBufferToString(data));
      chrome.test.succeed("Received feature report.");
    });
  });
}

function testReceiveFeatureReportWithoutReportId() {
  openDeviceWithoutReportId(function (connection) {
    chrome.hid.receiveFeatureReport(connection, 0, function (data) {
      chrome.test.assertNoLastError();
      var expected = "This is a HID feature report.";
      chrome.test.assertEq(expected, arrayBufferToString(data));
      chrome.test.succeed("Received feature report.");
    });
  });
}

function testReceiveFeatureReportWithInvalidReportId() {
  openDeviceWithReportId(function (connection) {
    chrome.hid.receiveFeatureReport(connection, 0, function (data) {
      chrome.test.assertLastError("Transfer failed.");
      chrome.test.succeed("Caught invalid report ID.");
    });
  });
}

function testReceiveFeatureReportWithUnexpectedReportId() {
  openDeviceWithoutReportId(function (connection) {
    chrome.hid.receiveFeatureReport(connection, 1, function (data) {
      chrome.test.assertLastError("Transfer failed.");
      chrome.test.succeed("Caught unexpected report ID.");
    });
  });
}

function testSendFeatureReportWithInvalidConnectionId() {
  var buffer = new ArrayBuffer();
  chrome.hid.sendFeatureReport(kInvalidConnectionId, 0, buffer, function () {
    chrome.test.assertLastError("Connection not established.");
    chrome.test.succeed("Rejected invalid connection ID.");
  });
}

function testSendFeatureReportWithReportId() {
  openDeviceWithReportId(function (connection) {
    var buffer =
        stringToArrayBuffer("The app is setting this HID feature report.");
    chrome.hid.sendFeatureReport(connection, 1, buffer, function () {
      chrome.test.assertNoLastError();
      chrome.hid.disconnect(connection);
      chrome.test.succeed("Send successful.");
    });
  });
}

function testSendFeatureReportWithoutReportId() {
  openDeviceWithoutReportId(function (connection) {
    var buffer =
        stringToArrayBuffer("The app is setting this HID feature report.");
    chrome.hid.sendFeatureReport(connection, 0, buffer, function () {
      chrome.test.assertNoLastError();
      chrome.hid.disconnect(connection);
      chrome.test.succeed("Send successful.");
    });
  });
}

function testSendFeatureReportWithInvalidReportId() {
  openDeviceWithReportId(function (connection) {
    var buffer =
        stringToArrayBuffer("The app is setting this HID feature report.");
    chrome.hid.sendFeatureReport(connection, 0, buffer, function () {
      chrome.test.assertLastError("Transfer failed.");
      chrome.hid.disconnect(connection);
      chrome.test.succeed("Caught invalid report ID.");
    });
  });
}

function testSendFeatureReportWithUnexpectedReportId() {
  openDeviceWithoutReportId(function (connection) {
    var buffer =
        stringToArrayBuffer("The app is setting this HID feature report.");
    chrome.hid.sendFeatureReport(connection, 1, buffer, function () {
      chrome.test.assertLastError("Transfer failed.");
      chrome.hid.disconnect(connection);
      chrome.test.succeed("Caught unexpected report ID.");
    });
  });
}

chrome.test.runTests([
  testGetDevicesWithNoOptions,
  testGetDevicesWithLegacyVidAndPid,
  testGetDevicesWithNoFilters,
  testGetDevicesWithVidPidFilter,
  testGetDevicesWithUsageFilter,
  testGetDevicesWithUnauthorizedDevice,
  testConnectWithInvalidDeviceId,
  testConnectAndDisconnect,
  testDisconnectWithInvalidConnectionId,
  testReceiveWithInvalidConnectionId,
  testReceiveWithReportId,
  testReceiveWithoutReportId,
  testSendWithInvalidConnectionId,
  testSendWithReportId,
  testSendWithoutReportId,
  testSendWithInvalidReportId,
  testSendWithUnexpectedReportId,
  testReceiveFeatureReportWithInvalidConnectionId,
  testReceiveFeatureReportWithReportId,
  testReceiveFeatureReportWithoutReportId,
  testReceiveFeatureReportWithInvalidReportId,
  testReceiveFeatureReportWithUnexpectedReportId,
  testSendFeatureReportWithInvalidConnectionId,
  testSendFeatureReportWithReportId,
  testSendFeatureReportWithoutReportId,
  testSendFeatureReportWithInvalidReportId,
  testSendFeatureReportWithUnexpectedReportId,
]);
