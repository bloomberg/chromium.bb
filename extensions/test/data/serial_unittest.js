// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Unit tests for the JS serial service client.
 *
 * These test that configuration and data are correctly transmitted between the
 * client and the service. They are launched by
 * extensions/renderer/api/serial/serial_api_unittest.cc.
 */

var test = require('test').binding;
var serial = require('serial').binding;
var unittestBindings = require('test_environment_specific_bindings');
var utils = require('utils');

var timeoutManager = new unittestBindings.TimeoutManager();
timeoutManager.installGlobals();

var BUFFER_SIZE = 10;

var connectionId = null;

function connect(callback, options) {
  options = options || {
    name: 'test connection',
    bufferSize: BUFFER_SIZE,
    receiveTimeout: 12345,
    sendTimeout: 6789,
    persistent: true,
  };
  serial.connect('device', options, test.callbackPass(function(connectionInfo) {
    connectionId = connectionInfo.connectionId;
    if (callback)
      callback(connectionInfo);
  }));
}

// Sets a function to be called once when data is received. Returns a promise
// that will resolve once the hook is installed.
function addReceiveHook(callback) {
  return requireAsync('serial_service').then(function(serialService) {
    var called = false;
    var dataReceived = serialService.Connection.prototype.onDataReceived_;
    serialService.Connection.prototype.onDataReceived_ = function() {
      var result = $Function.apply(dataReceived, this, arguments);
      if (!called)
        callback();
      called = true;
      return result;
    };
  });
}

// Sets a function to be called once when a receive error is received. Returns a
// promise that will resolve once the hook is installed.
function addReceiveErrorHook(callback) {
  return requireAsync('serial_service').then(function(serialService) {
    var called = false;
    var receiveError = serialService.Connection.prototype.onReceiveError_;
    serialService.Connection.prototype.onReceiveError_ = function() {
      var result = $Function.apply(receiveError, this, arguments);
      if (!called)
        callback();
      called = true;
      return result;
    };
  });
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
  test.assertEq(BUFFER_SIZE, connectionInfo.bufferSize);
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

function runReceiveErrorTest(expectedError) {
  connect();
  test.listenOnce(serial.onReceiveError, function(result) {
    serial.getInfo(connectionId, test.callbackPass(function(connectionInfo) {
      disconnect();
      test.assertTrue(connectionInfo.paused);
    }));
    test.assertEq(connectionId, result.connectionId);
    test.assertEq(expectedError, result.error);
  });
}

function runSendErrorTest(expectedError) {
  connect(function() {
    var buffer = new ArrayBuffer(1);
    serial.send(connectionId, buffer, test.callbackPass(function(sendInfo) {
      disconnect();
      test.assertEq(0, sendInfo.bytesSent);
      test.assertEq(expectedError, sendInfo.error);
    }));
  });
}

function sendData() {
  var data = 'data';
  var buffer = new ArrayBuffer(data.length);
  var byteBuffer = new Int8Array(buffer);
  for (var i = 0; i < data.length; i++) {
    byteBuffer[i] = data.charCodeAt(i);
  }
  return utils.promise(serial.send, connectionId, buffer);
}

function checkReceivedData(result) {
  var data = 'data';
  test.assertEq(connectionId, result.connectionId);
  test.assertEq(data.length, result.data.byteLength);
  var resultByteBuffer = new Int8Array(result.data);
  for (var i = 0; i < data.length; i++) {
    test.assertEq(data.charCodeAt(i), resultByteBuffer[i]);
  }
}

unittestBindings.exportTests([
  // Test that getDevices correctly transforms the data returned by the
  // SerialDeviceEnumerator.
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

  // Test that the correct error message is returned when an error occurs in
  // connecting to the port. This test uses an IoHandler that fails to connect.
  function testConnectFail() {
    serial.connect('device',
                   test.callbackFail('Failed to connect to the port.'));
  },

  // Test that the correct error message is returned when an error occurs in
  // calling getPortInfo after connecting to the port. This test uses an
  // IoHandler that fails on calls to GetPortInfo.
  function testGetInfoFailOnConnect() {
    serial.connect('device',
                   test.callbackFail('Failed to connect to the port.'));
  },

  // Test that the correct error message is returned when an invalid bit-rate
  // value is passed to connect.
  function testConnectInvalidBitrate() {
    serial.connect('device', {bitrate: -1}, test.callbackFail(
        'Failed to connect to the port.'));
  },

  // Test that a successful connect returns the expected connection info.
  function testConnect() {
    connect(function(connectionInfo) {
      disconnect();
      checkConnectionInfo(connectionInfo);
    });
  },

  // Test that a connection created with no options has the correct default
  // options.
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

  // Test that a getInfo call correctly converts the service-side info from the
  // Mojo format and returns both it and the client-side configuration.
  function testGetInfo() {
    connect(function() {
      serial.getInfo(connectionId,
                     test.callbackPass(function(connectionInfo) {
        disconnect();
        checkConnectionInfo(connectionInfo);
      }));
    });
  },

  // Test that only client-side options are returned when the service fails a
  // getInfo call. This test uses an IoHandler that fails GetPortInfo calls
  // after the initial call during connect.
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

  // Test that getConnections returns an array containing the open connection.
  function testGetConnections() {
    connect(function() {
      serial.getConnections(test.callbackPass(function(connections) {
        disconnect();
        test.assertEq(1, connections.length);
        checkConnectionInfo(connections[0]);
      }));
    });
  },

  // Test that getControlSignals correctly converts the Mojo format result. This
  // test uses an IoHandler that returns values matching the pattern being
  // tested.
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

  // Test that setControlSignals correctly converts to the Mojo format result.
  // This test uses an IoHandler that returns values following the same table of
  // values as |signalsValues|.
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

  // Test that update correctly passes values to the service only for
  // service-side options and that all update calls are reflected by the result
  // of getInfo calls. This test uses an IoHandler that expects corresponding
  // ConfigurePort calls.
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

  // Test that passing an invalid bit-rate reslts in an error.
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

  // Test flush. This test uses an IoHandler that counts the number of flush
  // calls.
  function testFlush() {
    connect(function() {
      serial.flush(connectionId, test.callbackPass(function(success) {
        disconnect();
        test.assertTrue(success);
      }));
    });
  },

  // Test that setPaused values are reflected by the results returned by getInfo
  // calls.
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

  // Test that a send and a receive correctly echoes data. This uses an
  // IoHandler that echoes data sent to it.
  function testEcho() {
    connect(function() {
      sendData().then(test.callbackPass(function(sendInfo) {
        test.assertEq(4, sendInfo.bytesSent);
        test.assertEq(undefined, sendInfo.error);
      }));
      test.listenOnce(serial.onReceive, function(result) {
        checkReceivedData(result);
        disconnect();
      });
    });
  },

  // Test that a send while another send is in progress returns a pending error.
  function testSendDuringExistingSend() {
    connect(function() {
      sendData().then(test.callbackPass(function(sendInfo) {
        test.assertEq(4, sendInfo.bytesSent);
        test.assertEq(undefined, sendInfo.error);
        disconnect();
      }));
      sendData().then(test.callbackPass(function(sendInfo) {
        test.assertEq(0, sendInfo.bytesSent);
        test.assertEq('pending', sendInfo.error);
      }));
    });
  },

  // Test that a second send after the first finishes is successful. This uses
  // an IoHandler that echoes data sent to it.
  function testSendAfterSuccessfulSend() {
    connect(function() {
      sendData().then(test.callbackPass(function(sendInfo) {
        test.assertEq(4, sendInfo.bytesSent);
        test.assertEq(undefined, sendInfo.error);
        return sendData();
      })).then(test.callbackPass(function(sendInfo) {
        test.assertEq(4, sendInfo.bytesSent);
        test.assertEq(undefined, sendInfo.error);
      }));
      // Check that the correct data is echoed twice.
      test.listenOnce(serial.onReceive, function(result) {
        checkReceivedData(result);
        test.listenOnce(serial.onReceive, function(result) {
          checkReceivedData(result);
          disconnect();
        });
      });
    });
  },

  // Test that a second send after the first fails is successful. This uses an
  // IoHandler that returns system_error for only the first send.
  function testSendPartialSuccessWithError() {
    connect(function() {
      sendData().then(test.callbackPass(function(sendInfo) {
        test.assertEq(2, sendInfo.bytesSent);
        test.assertEq('system_error', sendInfo.error);
        return sendData();
      })).then(test.callbackPass(function(sendInfo) {
        test.assertEq(4, sendInfo.bytesSent);
        test.assertEq(undefined, sendInfo.error);
        disconnect();
      }));
    });
  },

  // Test that a timed-out send returns a timeout error and that changing the
  // send timeout during a send does not affect its timeout. This test uses an
  // IoHandle that never completes sends.
  function testSendTimeout() {
    connect(function() {
      sendData().then(test.callbackPass(function(sendInfo) {
        test.assertEq(0, sendInfo.bytesSent);
        test.assertEq('timeout', sendInfo.error);
        test.assertEq(5, timeoutManager.currentTime);
        disconnect();
      }));
      serial.update(connectionId, {sendTimeout: 10}, test.callbackPass(
          timeoutManager.run.bind(timeoutManager, 1)));
    }, {sendTimeout: 5});
  },

  // Test that a timed-out send returns a timeout error and that disabling the
  // send timeout during a send does not affect its timeout. This test uses an
  // IoHandle that never completes sends.
  function testDisableSendTimeout() {
    connect(function() {
      sendData().then(test.callbackPass(function(sendInfo) {
        test.assertEq(0, sendInfo.bytesSent);
        test.assertEq('timeout', sendInfo.error);
        test.assertEq(6, timeoutManager.currentTime);
        disconnect();
      }));
      serial.update(connectionId, {sendTimeout: 0}, test.callbackPass(
          timeoutManager.run.bind(timeoutManager, 1)));
    }, {sendTimeout: 6});
  },

  // Test that data received while the connection is paused is queued and
  // dispatched once the connection is unpaused.
  function testPausedReceive() {
    // Wait until the receive hook is installed, then start the test.
    addReceiveHook(function() {
      // Unpause the connection after the connection has queued the received
      // data to ensure the queued data is dispatched when the connection is
      // unpaused.
      serial.setPaused(connectionId, false, test.callbackPass());
      // Check that setPaused(false) is idempotent.
      serial.setPaused(connectionId, false, test.callbackPass());
    }).then(function() {
      connect(function() {
        // Check that setPaused(true) is idempotent.
        serial.setPaused(connectionId, true, test.callbackPass());
        serial.setPaused(connectionId, true, test.callbackPass());
      });
    });
    test.listenOnce(serial.onReceive, function(result) {
      checkReceivedData(result);
      disconnect();
    });
  },

  // Test that a receive error received while the connection is paused is queued
  // and dispatched once the connection is unpaused.
  function testPausedReceiveError() {
    addReceiveErrorHook(function() {
      // Unpause the connection after the connection has queued the receive
      // error to ensure the queued error is dispatched when the connection is
      // unpaused.
      serial.setPaused(connectionId, false, test.callbackPass());
    }).then(test.callbackPass(function() {
      connect(function() {
        serial.setPaused(connectionId, true, test.callbackPass());
      });
    }));

    test.listenOnce(serial.onReceiveError, function(result) {
      serial.getInfo(connectionId, test.callbackPass(function(connectionInfo) {
        disconnect();
        test.assertTrue(connectionInfo.paused);
      }));
      test.assertEq(connectionId, result.connectionId);
      test.assertEq('device_lost', result.error);
    });
    serial.onReceive.addListener(function() {
      test.fail('unexpected onReceive event');
    });
  },

  // Test that receive timeouts trigger after the timeout time elapses and that
  // changing the receive timeout does not affect a wait in progress.
  function testReceiveTimeout() {
    connect(function() {
      test.listenOnce(serial.onReceiveError, function(result) {
        test.assertEq(connectionId, result.connectionId);
        test.assertEq('timeout', result.error);
        test.assertEq(20, timeoutManager.currentTime);
        serial.getInfo(connectionId, test.callbackPass(
            function(connectionInfo) {
          test.assertFalse(connectionInfo.paused);
          disconnect();
        }));
      });
      // Changing the timeout does not take effect until the current timeout
      // expires or a receive completes.
      serial.update(connectionId, {receiveTimeout: 10}, test.callbackPass(
          timeoutManager.run.bind(timeoutManager, 1)));
    }, {receiveTimeout: 20});
  },

  // Test that receive timeouts trigger after the timeout time elapses and that
  // disabling the receive timeout does not affect a wait in progress.
  function testDisableReceiveTimeout() {
    connect(function() {
      test.listenOnce(serial.onReceiveError, function(result) {
        test.assertEq(connectionId, result.connectionId);
        test.assertEq('timeout', result.error);
        test.assertEq(30, timeoutManager.currentTime);
        serial.getInfo(connectionId, test.callbackPass(
            function(connectionInfo) {
          disconnect();
          test.assertFalse(connectionInfo.paused);
        }));
      });
      // Disabling the timeout does not take effect until the current timeout
      // expires or a receive completes.
      serial.update(connectionId, {receiveTimeout: 0}, test.callbackPass(
          timeoutManager.run.bind(timeoutManager, 1)));
    }, {receiveTimeout: 30});
  },

  // Test that a receive error from the service is correctly dispatched. This
  // test uses an IoHandler that only reports 'disconnected' receive errors.
  function testReceiveErrorDisconnected() {
    runReceiveErrorTest('disconnected');
  },

  // Test that a receive error from the service is correctly dispatched. This
  // test uses an IoHandler that only reports 'device_lost' receive errors.
  function testReceiveErrorDeviceLost() {
    runReceiveErrorTest('device_lost');
  },

  // Test that a receive from error the service is correctly dispatched. This
  // test uses an IoHandler that only reports 'system_error' receive errors.
  function testReceiveErrorSystemError() {
    runReceiveErrorTest('system_error');
  },

  // Test that a send error from the service is correctly returned as the send
  // result. This test uses an IoHandler that only reports 'disconnected' send
  // errors.
  function testSendErrorDisconnected() {
    runSendErrorTest('disconnected');
  },

  // Test that a send error from the service is correctly returned as the send
  // result. This test uses an IoHandler that only reports 'system_error' send
  // errors.
  function testSendErrorSystemError() {
    runSendErrorTest('system_error');
  },

  // Test that disconnect returns the correct error for a connection ID that
  // does not exist.
  function testDisconnectUnknownConnectionId() {
    serial.disconnect(-1, test.callbackFail('Serial connection not found.'));
  },

  // Test that getInfo returns the correct error for a connection ID that does
  // not exist.
  function testGetInfoUnknownConnectionId() {
    serial.getInfo(-1, test.callbackFail('Serial connection not found.'));
  },

  // Test that update returns the correct error for a connection ID that does
  // not exist.
  function testUpdateUnknownConnectionId() {
    serial.update(-1, {}, test.callbackFail('Serial connection not found.'));
  },

  // Test that setControlSignals returns the correct error for a connection ID
  // that does not exist.
  function testSetControlSignalsUnknownConnectionId() {
    serial.setControlSignals(-1, {}, test.callbackFail(
        'Serial connection not found.'));
  },

  // Test that getControlSignals returns the correct error for a connection ID
  // that does not exist.
  function testGetControlSignalsUnknownConnectionId() {
    serial.getControlSignals(-1, test.callbackFail(
        'Serial connection not found.'));
  },

  // Test that flush returns the correct error for a connection ID that does not
  // exist.
  function testFlushUnknownConnectionId() {
    serial.flush(-1, test.callbackFail('Serial connection not found.'));
  },

  // Test that setPaused returns the correct error for a connection ID that does
  // not exist.
  function testSetPausedUnknownConnectionId() {
    serial.setPaused(
        -1, true, test.callbackFail('Serial connection not found.'));
    serial.setPaused(
        -1, false, test.callbackFail('Serial connection not found.'));
  },

  // Test that send returns the correct error for a connection ID that does not
  // exist.
  function testSendUnknownConnectionId() {
    var buffer = new ArrayBuffer(1);
    serial.send(-1, buffer, test.callbackFail('Serial connection not found.'));
  },
], test.runTests, exports);
