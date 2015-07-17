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

var OPTIONS_VALUES = [
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

// Create a serial connection. That serial connection will be used by the other
// helper functions below.
function connect(options) {
  options = options || {
    name: 'test connection',
    bufferSize: BUFFER_SIZE,
    receiveTimeout: 12345,
    sendTimeout: 6789,
    persistent: true,
  };
  return utils.promise(serial.connect, 'device', options).then(function(info) {
    connectionId = info.connectionId;
    return info;
  });
}

// Serialize and deserialize all serial connections, preserving onData and
// onError event listeners.
function serializeRoundTrip() {
  return requireAsync('serial_service').then(function(serialService) {
    function serializeConnections(connections) {
      var serializedConnections = [];
      for (var connection of connections.values()) {
        serializedConnections.push(serializeConnection(connection));
      }
      return Promise.all(serializedConnections);
    }

    function serializeConnection(connection) {
      var onData = connection.onData;
      var onError = connection.onError;
      return connection.serialize().then(function(serialization) {
        return {
          serialization: serialization,
          onData: onData,
          onError: onError,
        };
      });
    }

    function deserializeConnections(serializedConnections) {
      $Array.forEach(serializedConnections, function(serializedConnection) {
        var connection = serialService.Connection.deserialize(
            serializedConnection.serialization);
        connection.onData = serializedConnection.onData;
        connection.onError = serializedConnection.onError;
        connection.resumeReceives();
      });
    }

    return serialService.getConnections()
        .then(serializeConnections)
        .then(deserializeConnections);
  });
}

// Returns a promise that will resolve to the connection info for the
// connection.
function getInfo() {
  return utils.promise(serial.getInfo, connectionId);
}

// Returns a function that checks that the values of keys contained within
// |expectedInfo| match the values of the same keys contained within |info|.
function checkInfo(expectedInfo) {
  return function(info) {
    for (var key in expectedInfo) {
      test.assertEq(expectedInfo[key], info[key]);
    }
  };
}

// Returns a function that will update the options of the serial connection with
// those contained within |values|.
function update(values) {
  return function() {
    return utils.promise(serial.update, connectionId, values);
  };
}

// Checks that the previous operation succeeded.
function expectSuccess(success) {
  test.assertTrue(success);
}

// Returns a function that checks that the send result matches |bytesSent| and
// |error|. If no error is expected, |error| may be omitted.
function expectSendResult(bytesSent, error) {
  return function(sendInfo) {
    test.assertEq(bytesSent, sendInfo.bytesSent);
    test.assertEq(error, sendInfo.error);
  };
}

// Returns a function that checks that the current time is |expectedTime|.
function expectCurrentTime(expectedTime) {
  return function() {
    test.assertEq(expectedTime, timeoutManager.currentTime);
  }
}

// Returns a promise that will resolve to the device control signals for the
// serial connection.
function getControlSignals() {
  return utils.promise(serial.getControlSignals, connectionId);
}

// Returns a function that will set the control signals for the serial
// connection to |signals|.
function setControlSignals(signals) {
  return function() {
    return utils.promise(serial.setControlSignals, connectionId, signals);
  };
}

// Returns a function that will set the paused state of the serial connection to
// |paused|.
function setPaused(paused) {
  return function() {
    return utils.promise(serial.setPaused, connectionId, paused);
  }
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

function listenOnce(targetEvent) {
  return new Promise(function(resolve, reject) {
    targetEvent.addListener(function(result) {
      resolve(result);
    });
  });
}

function disconnect() {
  return utils.promise(serial.disconnect, connectionId).then(function(success) {
    test.assertTrue(success);
    connectionId = null;
  });
}

function checkClientConnectionInfo(connectionInfo) {
  test.assertTrue(connectionInfo.persistent);
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
  test.assertEq(12, $Object.keys(connectionInfo).length);
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

function checkReceiveError(expectedError) {
  return function(result) {
    test.assertEq(connectionId, result.connectionId);
    test.assertEq(expectedError, result.error);
  }
}

function runReceiveErrorTest(expectedError) {
  var errorReceived = listenOnce(serial.onReceiveError);
  Promise.all([
      connect(),
      errorReceived
          .then(checkReceiveError(expectedError)),
      errorReceived
          .then(getInfo)
          .then(checkInfo({paused: true})),
  ])
      .then(disconnect)
      .then(test.succeed, test.fail);
}

function runSendErrorTest(expectedError) {
  connect()
      .then(sendData)
      .then(expectSendResult(0, expectedError))
      .then(disconnect)
      .then(test.succeed, test.fail);
}

unittestBindings.exportTests([
  // Test that getDevices correctly transforms the data returned by the
  // SerialDeviceEnumerator.
  function testGetDevices() {
    utils.promise(serial.getDevices).then(function(devices) {
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
    }).then(test.succeed, test.fail);
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
    connect()
        .then(checkConnectionInfo)
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that a connection created with no options has the correct default
  // options.
  function testConnectDefaultOptions() {
    connect({}).then(function(connectionInfo) {
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
    })
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that a getInfo call correctly converts the service-side info from the
  // Mojo format and returns both it and the client-side configuration.
  function testGetInfo() {
    connect()
        .then(getInfo)
        .then(checkConnectionInfo)
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that a getInfo call returns the correct info after serialization.
  function testGetInfoAfterSerialization() {
    connect()
        .then(serializeRoundTrip)
        .then(getInfo)
        .then(checkConnectionInfo)
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that only client-side options are returned when the service fails a
  // getInfo call. This test uses an IoHandler that fails GetPortInfo calls
  // after the initial call during connect.
  function testGetInfoFailToGetPortInfo() {
    var info = connect().then(getInfo);
    Promise.all([
        info.then(function(connectionInfo) {
          test.assertFalse('bitrate' in connectionInfo);
          test.assertFalse('dataBits' in connectionInfo);
          test.assertFalse('parityBit' in connectionInfo);
          test.assertFalse('stopBit' in connectionInfo);
          test.assertFalse('ctsFlowControl' in connectionInfo);
        }),
        info.then(checkClientConnectionInfo),
    ])
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that getConnections returns an array containing the open connection.
  function testGetConnections() {
    connect().then(function() {
      return utils.promise(serial.getConnections);
    }).then(function(connections) {
      test.assertEq(1, connections.length);
      checkConnectionInfo(connections[0]);
    })
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that getControlSignals correctly converts the Mojo format result. This
  // test uses an IoHandler that returns values matching the pattern being
  // tested.
  function testGetControlSignals() {
    function checkControlSignals(expectedBitfield) {
      return function(signals) {
        test.assertEq(!!(expectedBitfield & 1), signals.dcd);
        test.assertEq(!!(expectedBitfield & 2), signals.cts);
        test.assertEq(!!(expectedBitfield & 4), signals.ri);
        test.assertEq(!!(expectedBitfield & 8), signals.dsr);
      };
    }
    var promiseChain = connect();
    for (var i = 0; i < 16; i++) {
      promiseChain = promiseChain
          .then(getControlSignals)
          .then(checkControlSignals(i));
    }
    promiseChain
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that setControlSignals correctly converts to the Mojo format result.
  // This test uses an IoHandler that returns values following the same table of
  // values as |signalsValues|.
  function testSetControlSignals() {
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
    var promiseChain = connect();
    for (var i = 0; i < signalsValues.length; i++) {
      promiseChain = promiseChain.then(setControlSignals(signalsValues[i]));
    }
    promiseChain
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that update correctly passes values to the service only for
  // service-side options and that all update calls are reflected by the result
  // of getInfo calls. This test uses an IoHandler that expects corresponding
  // ConfigurePort calls.
  function testUpdate() {
    var promiseChain = connect()
        .then(getInfo)
        .then(checkInfo(OPTIONS_VALUES[i]));
    for (var i = 1; i < OPTIONS_VALUES.length; i++) {
      promiseChain = promiseChain
          .then(update(OPTIONS_VALUES[i]))
          .then(expectSuccess)
          .then(getInfo)
          .then(checkInfo(OPTIONS_VALUES[i]));
    }
    promiseChain
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that options set by update persist after serialization.
  function testUpdateAcrossSerialization() {
    var promiseChain = connect()
        .then(serializeRoundTrip)
        .then(getInfo)
        .then(checkInfo(OPTIONS_VALUES[i]));
    for (var i = 1; i < OPTIONS_VALUES.length; i++) {
      promiseChain = promiseChain
          .then(update(OPTIONS_VALUES[i]))
          .then(expectSuccess)
          .then(serializeRoundTrip)
          .then(getInfo)
          .then(checkInfo(OPTIONS_VALUES[i]));
    }
    promiseChain
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that passing an invalid bit-rate reslts in an error.
  function testUpdateInvalidBitrate() {
    connect()
        .then(update({bitrate: -1}))
        .then(function(success) {
      test.assertFalse(success);
    })
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test flush. This test uses an IoHandler that counts the number of flush
  // calls.
  function testFlush() {
    connect().then(function() {
      return utils.promise(serial.flush, connectionId);
    })
        .then(expectSuccess)
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that setPaused values are reflected by the results returned by getInfo
  // calls.
  function testSetPaused() {
    connect()
        .then(setPaused(true))
        .then(getInfo)
        .then(checkInfo({paused: true}))
        .then(setPaused(false))
        .then(getInfo)
        .then(checkInfo({paused: false}))
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that a send and a receive correctly echoes data. This uses an
  // IoHandler that echoes data sent to it.
  function testEcho() {
    Promise.all([
        connect()
            .then(sendData)
            .then(expectSendResult(4)),
        listenOnce(serial.onReceive)
            .then(checkReceivedData),
    ])
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that a send while another send is in progress returns a pending error.
  function testSendDuringExistingSend() {
    var connected = connect();
    Promise.all([
        connected
            .then(sendData)
            .then(expectSendResult(4)),
        connected
            .then(sendData)
            .then(expectSendResult(0, 'pending')),
    ])
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that a second send after the first finishes is successful. This uses
  // an IoHandler that echoes data sent to it.
  function testSendAfterSuccessfulSend() {
    connect()
        .then(sendData)
        .then(expectSendResult(4))
        .then(sendData)
        .then(expectSendResult(4))
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that a second send after the first fails is successful. This uses an
  // IoHandler that returns system_error for only the first send.
  function testSendPartialSuccessWithError() {
    connect()
        .then(sendData)
        .then(expectSendResult(2, 'system_error'))
        .then(sendData)
        .then(expectSendResult(4))
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that a send and a receive correctly echoes data after serialization.
  function testEchoAfterSerialization() {
    Promise.all([
        connect()
            .then(serializeRoundTrip)
            .then(sendData)
            .then(expectSendResult(4)),
        listenOnce(serial.onReceive).then(checkReceivedData)
    ])
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that a timed-out send returns a timeout error and that changing the
  // send timeout during a send does not affect its timeout. This test uses an
  // IoHandle that never completes sends.
  function testSendTimeout() {
    var connected = connect({sendTimeout: 5});
    var sent = connected.then(sendData);
    Promise.all([
        sent.then(expectSendResult(0, 'timeout')),
        sent.then(expectCurrentTime(5)),
        connected.then(update({sendTimeout: 10}))
            .then(expectSuccess)
            .then(timeoutManager.run.bind(timeoutManager, 1)),
    ])
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that send timeouts still function correctly after a serialization
  // round trip.
  function testSendTimeoutAfterSerialization() {
    var connected = connect({sendTimeout: 5}).then(serializeRoundTrip);
    var sent = connected.then(sendData);
    Promise.all([
        sent.then(expectSendResult(0, 'timeout')),
        sent.then(expectCurrentTime(5)),
        connected.then(update({sendTimeout: 10}))
            .then(expectSuccess)
            .then(timeoutManager.run.bind(timeoutManager, 1)),
    ])
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that a timed-out send returns a timeout error and that disabling the
  // send timeout during a send does not affect its timeout. This test uses an
  // IoHandle that never completes sends.
  function testDisableSendTimeout() {
    var connected = connect({sendTimeout: 5});
    var sent = connected.then(sendData);
    Promise.all([
        sent.then(expectSendResult(0, 'timeout')),
        sent.then(expectCurrentTime(5)),
        connected.then(update({sendTimeout: 0}))
            .then(expectSuccess)
            .then(timeoutManager.run.bind(timeoutManager, 1)),
    ])
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that data received while the connection is paused is queued and
  // dispatched once the connection is unpaused.
  function testPausedReceive() {
    Promise.all([
        // Wait until the receive hook is installed, then start the test.
        addReceiveHook(function() {
          // Unpause the connection after the connection has queued the received
          // data to ensure the queued data is dispatched when the connection is
          // unpaused.
          Promise.all([
              utils.promise(serial.setPaused, connectionId, false),
              // Check that setPaused(false) is idempotent.
              utils.promise(serial.setPaused, connectionId, false),
          ]).catch(test.fail);
        })
            .then(connect)
            .then(function() {
          // Check that setPaused(true) is idempotent.
          return Promise.all([
              utils.promise(serial.setPaused, connectionId, true),
              utils.promise(serial.setPaused, connectionId, true),
          ]);
        }),
        listenOnce(serial.onReceive).then(checkReceivedData),
    ])
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that a receive error received while the connection is paused is queued
  // and dispatched once the connection is unpaused.
  function testPausedReceiveError() {
    Promise.all([
        // Wait until the receive hook is installed, then start the test.
        addReceiveErrorHook(function() {
          // Unpause the connection after the connection has queued the received
          // data to ensure the queued data is dispatched when the connection is
          // unpaused.
          utils.promise(serial.setPaused, connectionId, false).catch(test.fail);
        })
            .then(connect)
            .then(setPaused(true)),
        listenOnce(serial.onReceiveError)
            .then(checkReceiveError('device_lost')),
    ])
        .then(disconnect)
        .then(test.succeed, test.fail);
    serial.onReceive.addListener(function() {
      test.fail('unexpected onReceive event');
    });
  },

  // Test that receive timeouts trigger after the timeout time elapses and that
  // changing the receive timeout does not affect a wait in progress.
  function testReceiveTimeout() {
    var errorReceived = listenOnce(serial.onReceiveError);
    Promise.all([
        errorReceived.then(checkReceiveError('timeout')),
        errorReceived.then(expectCurrentTime(20)),
        errorReceived
            .then(getInfo)
            .then(checkInfo({paused: false})),
        connect({receiveTimeout: 20})
            // Changing the timeout does not take effect until the current
            // timeout expires or a receive completes.
            .then(update({receiveTimeout: 10}))
            .then(expectSuccess)
            .then(timeoutManager.run.bind(timeoutManager, 1)),
    ])
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that receive timeouts still function correctly after a serialization
  // round trip.
  function testReceiveTimeoutAfterSerialization() {
    var errorReceived = listenOnce(serial.onReceiveError);
    Promise.all([
        errorReceived.then(checkReceiveError('timeout')),
        errorReceived.then(expectCurrentTime(20)),
        errorReceived
            .then(getInfo)
            .then(checkInfo({paused: false})),
        connect({receiveTimeout: 20})
            .then(serializeRoundTrip)
            .then(timeoutManager.run.bind(timeoutManager, 1)),
    ])
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  // Test that receive timeouts trigger after the timeout time elapses and that
  // disabling the receive timeout does not affect a wait in progress.
  function testDisableReceiveTimeout() {
    var errorReceived = listenOnce(serial.onReceiveError);
    Promise.all([
        errorReceived.then(checkReceiveError('timeout')),
        errorReceived.then(expectCurrentTime(20)),
        errorReceived
            .then(getInfo)
            .then(checkInfo({paused: false})),
        connect({receiveTimeout: 20})
            // Disabling the timeout does not take effect until the current
            // timeout expires or a receive completes.
            .then(update({receiveTimeout: 0}))
            .then(expectSuccess)
            .then(timeoutManager.run.bind(timeoutManager, 1)),
    ])
        .then(disconnect)
        .then(test.succeed, test.fail);
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
  // test uses an IoHandler that only reports 'break' receive errors.
  function testReceiveErrorBreak() {
    runReceiveErrorTest('break');
  },

  // Test that a receive from error the service is correctly dispatched. This
  // test uses an IoHandler that only reports 'frame_error' receive errors.
  function testReceiveErrorFrameError() {
    runReceiveErrorTest('frame_error');
  },

  // Test that a receive from error the service is correctly dispatched. This
  // test uses an IoHandler that only reports 'overrun' receive errors.
  function testReceiveErrorOverrun() {
    runReceiveErrorTest('overrun');
  },

  // Test that a receive from error the service is correctly dispatched. This
  // test uses an IoHandler that only reports 'buffer_overflow' receive errors.
  function testReceiveErrorBufferOverflow() {
    runReceiveErrorTest('buffer_overflow');
  },

  // Test that a receive from error the service is correctly dispatched. This
  // test uses an IoHandler that only reports 'parity_error' receive errors.
  function testReceiveErrorParityError() {
    runReceiveErrorTest('parity_error');
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

  function testSendAndStash() {
    connect()
        .then(setPaused(true))
        .then(sendData)
        .then(expectSendResult(4))
        .then(test.succeed, test.fail);
  },

  function testRestoreAndReceive() {
    connectionId = 0;
    Promise.all([
        utils.promise(serial.setPaused, connectionId, false),
        listenOnce(serial.onReceive).then(checkReceivedData),
    ])
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  function testRestoreAndReceiveErrorSetUp() {
    connect().then(test.succeed, test.fail);
  },

  function testRestoreAndReceiveError() {
    connectionId = 0;
    Promise.all([
        utils.promise(serial.setPaused, connectionId, false),
        listenOnce(serial.onReceiveError)
            .then(checkReceiveError('device_lost')),
    ])
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

  function testStashNoConnections() {
    connect({persistent: false}).then(test.succeed, test.fail);
  },

  function testRestoreNoConnections() {
    connect()
        .then(function(connectionInfo) {
          test.assertEq(0, connectionInfo.connectionId);
          return connectionInfo;
        })
        .then(checkConnectionInfo)
        .then(disconnect)
        .then(test.succeed, test.fail);
  },

], test.runTests, exports);
