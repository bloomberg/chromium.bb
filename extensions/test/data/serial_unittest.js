// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Unit tests for the JS serial service client.
 *
 * These test that configuration and data are correctly transmitted between the
 * client and the service.
 */

var test = require('test').binding;
var serial = require('serial').binding;
var unittestBindings = require('test_environment_specific_bindings');

var connectionId = null;

function connect(callback, options) {
  options = options || {
    name: 'test connection',
    bufferSize: 8192,
    receiveTimeout: 12345,
    sendTimeout: 6789,
    persistent: true,
  }
  serial.connect('device', options, test.callbackPass(function(connectionInfo) {
    connectionId = connectionInfo.connectionId;
    callback(connectionInfo);
  }));
}

function disconnect() {
  serial.disconnect(connectionId, test.callbackPass(function(success) {
    test.assertTrue(success);
    connectionId = null;
  }));
}

function checkClientConnectionInfo(connectionInfo) {
  test.assertFalse(connectionInfo.persistent);
  test.assertEq('test connection', connectionInfo.name);
  test.assertEq(12345, connectionInfo.receiveTimeout);
  test.assertEq(6789, connectionInfo.sendTimeout);
  test.assertEq(8192, connectionInfo.bufferSize);
  test.assertFalse(connectionInfo.paused);
}

function checkServiceConnectionInfo(connectionInfo) {
  test.assertEq(9600, connectionInfo.bitrate);
  test.assertEq('eight', connectionInfo.dataBits);
  test.assertEq('no', connectionInfo.parityBit);
  test.assertEq('one', connectionInfo.stopBits);
  test.assertFalse(connectionInfo.ctsFlowControl);
}

function checkConnectionInfo(connectionInfo) {
  checkClientConnectionInfo(connectionInfo);
  checkServiceConnectionInfo(connectionInfo);
}

unittestBindings.exportTests([
  function testGetDevices() {
    serial.getDevices(test.callbackPass(function(devices) {
      test.assertEq(3, devices.length);
      test.assertEq(4, $Object.keys(devices[0]).length);
      test.assertEq('device', devices[0].path);
      test.assertEq(1234, devices[0].vendorId);
      test.assertEq(5678, devices[0].productId);
      test.assertEq('foo', devices[0].displayName);
      test.assertEq(1, $Object.keys(devices[1]).length);
      test.assertEq('another_device', devices[1].path);
      test.assertEq(1, $Object.keys(devices[2]).length);
      test.assertEq('', devices[2].path);
    }));
  },

  function testConnectFail() {
    serial.connect('device',
                   test.callbackFail('Failed to connect to the port.'));
  },

  function testGetInfoFailOnConnect() {
    serial.connect('device',
                   test.callbackFail('Failed to connect to the port.'));
  },

  function testConnectInvalidBitrate() {
    serial.connect('device', {bitrate: -1}, test.callbackFail(
        'Failed to connect to the port.'));
  },

  function testConnect() {
    connect(function(connectionInfo) {
      disconnect();
      checkConnectionInfo(connectionInfo);
    });
  },

  function testConnectDefaultOptions() {
    connect(function(connectionInfo) {
      disconnect();
      test.assertEq(9600, connectionInfo.bitrate);
      test.assertEq('eight', connectionInfo.dataBits);
      test.assertEq('no', connectionInfo.parityBit);
      test.assertEq('one', connectionInfo.stopBits);
      test.assertFalse(connectionInfo.ctsFlowControl);
      test.assertFalse(connectionInfo.persistent);
      test.assertEq('', connectionInfo.name);
      test.assertEq(0, connectionInfo.receiveTimeout);
      test.assertEq(0, connectionInfo.sendTimeout);
      test.assertEq(4096, connectionInfo.bufferSize);
    }, {});
  },

  function testGetInfo() {
    connect(function() {
      serial.getInfo(connectionId,
                     test.callbackPass(function(connectionInfo) {
        disconnect();
        checkConnectionInfo(connectionInfo);
      }));
    });
  },

  function testGetInfoFailToGetPortInfo() {
    connect(function() {
      serial.getInfo(connectionId,
                     test.callbackPass(function(connectionInfo) {
        disconnect();
        checkClientConnectionInfo(connectionInfo);
        test.assertFalse('bitrate' in connectionInfo);
        test.assertFalse('dataBits' in connectionInfo);
        test.assertFalse('parityBit' in connectionInfo);
        test.assertFalse('stopBit' in connectionInfo);
        test.assertFalse('ctsFlowControl' in connectionInfo);
      }));
    });
  },

  function testGetConnections() {
    connect(function() {
      serial.getConnections(test.callbackPass(function(connections) {
        disconnect();
        test.assertEq(1, connections.length);
        checkConnectionInfo(connections[0]);
      }));
    });
  },

  function testGetControlSignals() {
    connect(function() {
      var calls = 0;
      function checkControlSignals(signals) {
        if (calls == 15) {
          disconnect();
        } else {
          serial.getControlSignals(
              connectionId,
              test.callbackPass(checkControlSignals));
        }
        test.assertEq(!!(calls & 1), signals.dcd);
        test.assertEq(!!(calls & 2), signals.cts);
        test.assertEq(!!(calls & 4), signals.ri);
        test.assertEq(!!(calls & 8), signals.dsr);
        calls++;
      }
      serial.getControlSignals(connectionId,
                               test.callbackPass(checkControlSignals));
    });
  },

  function testSetControlSignals() {
    connect(function() {
      var signalsValues = [
        {},
        {dtr: false},
        {dtr: true},
        {rts: false},
        {dtr: false, rts: false},
        {dtr: true, rts: false},
        {rts: true},
        {dtr: false, rts: true},
        {dtr: true, rts: true},
      ];
      var calls = 0;
      function setControlSignals(success) {
        if (calls == signalsValues.length) {
          disconnect();
        } else {
          serial.setControlSignals(connectionId,
                                   signalsValues[calls++],
                                   test.callbackPass(setControlSignals));
        }
        test.assertTrue(success);
      }
      setControlSignals(true);
    });
  },

  function testUpdate() {
    connect(function() {
      var optionsValues = [
        {},  // SetPortOptions is called once during connection.
        {bitrate: 57600},
        {dataBits: 'seven'},
        {dataBits: 'eight'},
        {parityBit: 'no'},
        {parityBit: 'odd'},
        {parityBit: 'even'},
        {stopBits: 'one'},
        {stopBits: 'two'},
        {ctsFlowControl: false},
        {ctsFlowControl: true},
        {bufferSize: 1},
        {sendTimeout: 0},
        {receiveTimeout: 0},
        {persistent: false},
        {name: 'name'},
      ];
      var calls = 0;
      function checkInfo(info) {
        for (var key in optionsValues[calls]) {
          test.assertEq(optionsValues[calls][key], info[key]);
        }
        setOptions();
      }
      function setOptions() {
        if (++calls == optionsValues.length) {
          disconnect();
        } else {
          serial.update(connectionId,
                        optionsValues[calls],
                        test.callbackPass(function(success) {
            serial.getInfo(connectionId, test.callbackPass(checkInfo));
            test.assertTrue(success);
          }));
        }
      }
      setOptions();
    });
  },

  function testUpdateInvalidBitrate() {
    connect(function() {
      serial.update(connectionId,
                    {bitrate: -1},
                    test.callbackPass(function(success) {
        disconnect();
        test.assertFalse(success);
      }));
    });
  },

  function testFlush() {
    connect(function() {
      serial.flush(connectionId,
                     test.callbackPass(function(success) {
        disconnect();
        test.assertTrue(success);
      }));
    });
  },

  function testSetPaused() {
    connect(function() {
      serial.setPaused(connectionId, true, test.callbackPass(function() {
        serial.getInfo(connectionId, test.callbackPass(function(info) {
          serial.setPaused(connectionId, false, test.callbackPass(function() {
            serial.getInfo(connectionId, test.callbackPass(function(info) {
              test.assertFalse(info.paused);
              disconnect();
            }));
          }));
          test.assertTrue(info.paused);
        }));
      }));
    });
  },

  function testDisconnectUnknownConnectionId() {
    serial.disconnect(-1, test.callbackFail('Serial connection not found.'));
  },

  function testGetInfoUnknownConnectionId() {
    serial.getInfo(-1, test.callbackFail('Serial connection not found.'));
  },

  function testUpdateUnknownConnectionId() {
    serial.update(-1, {}, test.callbackFail('Serial connection not found.'));
  },

  function testSetControlSignalsUnknownConnectionId() {
    serial.setControlSignals(-1, {}, test.callbackFail(
        'Serial connection not found.'));
  },

  function testGetControlSignalsUnknownConnectionId() {
    serial.getControlSignals(-1, test.callbackFail(
        'Serial connection not found.'));
  },

  function testFlushUnknownConnectionId() {
    serial.flush(-1, test.callbackFail('Serial connection not found.'));
  },

  function testSetPausedUnknownConnectionId() {
    serial.setPaused(
        -1, true, test.callbackFail('Serial connection not found.'));
    serial.setPaused(
        -1, false, test.callbackFail('Serial connection not found.'));
  },
], test.runTests, exports);
