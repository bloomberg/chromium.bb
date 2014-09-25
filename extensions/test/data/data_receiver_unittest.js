// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests launched by extensions/renderer/api/serial/data_receiver_unittest.cc

var test = require('test').binding;
var unittestBindings = require('test_environment_specific_bindings');

var BUFFER_SIZE = 10;
var FATAL_ERROR = 2;

// Returns a promise to a newly created DataReceiver.
function createReceiver() {
  return Promise.all([
    requireAsync('content/public/renderer/service_provider'),
    requireAsync('data_receiver'),
    requireAsync('device/serial/data_stream.mojom'),
  ]).then(function(modules) {
    var serviceProvider = modules[0];
    var dataReceiver = modules[1];
    var dataStream = modules[2];
    return new dataReceiver.DataReceiver(
        serviceProvider.connectToService(dataStream.DataSourceProxy.NAME_),
        BUFFER_SIZE,
        FATAL_ERROR);
  });
}

// Returns a promise that will resolve to |receiver| when it has received an
// error from its DataSource.
function waitForReceiveError(receiver) {
  return new Promise(function(resolve, reject) {
    var onError = receiver.onError;
    receiver.onError = function() {
      $Function.apply(onError, receiver, arguments);
      resolve(receiver);
    };
  });
}

// Returns a function that receives data from a provided DataReceiver
// |receiver|, checks that it matches the expected data and returns a promise
// that will resolve to |receiver|.
function receiveAndCheckData(expectedData) {
  return function(receiver) {
    return receiver.receive().then(function(data) {
      test.assertEq(expectedData.length, data.byteLength);
      for (var i = 0; i < expectedData.length; i++)
        test.assertEq(expectedData.charCodeAt(i), new Int8Array(data)[i]);
      return receiver;
    });
    test.assertThrows(
        receiver.receive, receiver, [], 'Receive already in progress.');
  };
}

// Returns a function that attempts to receive data from a provided DataReceiver
// |receiver|, checks that the correct error is reported and returns a promise
// that will resolve to |receiver|.
function receiveAndCheckError(expectedError) {
  return function(receiver) {
    return receiver.receive().catch(function(error) {
      test.assertEq(expectedError, error.error);
      return receiver;
    });
    test.assertThrows(
        receiver.receive, receiver, [], 'Receive already in progress.');
  };
}

// Serializes and deserializes the provided DataReceiver |receiver|, returning
// a promise that will resolve to the newly deserialized DataReceiver.
function serializeRoundTrip(receiver) {
  return Promise.all([
    receiver.serialize(),
    requireAsync('data_receiver'),
  ]).then(function(promises) {
    var serialized = promises[0];
    var dataReceiverModule = promises[1];
    return dataReceiverModule.DataReceiver.deserialize(serialized);
  });
}

// Closes and returns the provided DataReceiver |receiver|.
function closeReceiver(receiver) {
  receiver.close();
  return receiver;
}

unittestBindings.exportTests([
  function testReceive() {
    createReceiver()
        .then(receiveAndCheckData('a'))
        .then(closeReceiver)
        .then(test.succeed, test.fail);
  },

  function testReceiveError() {
    createReceiver()
        .then(receiveAndCheckError(1))
        .then(closeReceiver)
        .then(test.succeed, test.fail);
  },

  function testReceiveDataAndError() {
    createReceiver()
        .then(receiveAndCheckData('a'))
        .then(receiveAndCheckError(1))
        .then(receiveAndCheckData('b'))
        .then(closeReceiver)
        .then(test.succeed, test.fail);
  },

  function testReceiveErrorThenData() {
    createReceiver()
        .then(receiveAndCheckError(1))
        .then(receiveAndCheckData('a'))
        .then(closeReceiver)
        .then(test.succeed, test.fail);
  },

  function testReceiveBeforeAndAfterSerialization() {
    createReceiver()
        .then(receiveAndCheckData('a'))
        .then(serializeRoundTrip)
        .then(receiveAndCheckData('b'))
        .then(closeReceiver)
        .then(test.succeed, test.fail);
  },

  function testReceiveErrorSerialization() {
    createReceiver()
        .then(waitForReceiveError)
        .then(serializeRoundTrip)
        .then(receiveAndCheckError(1))
        .then(receiveAndCheckError(3))
        .then(closeReceiver)
        .then(test.succeed, test.fail);
  },

  function testReceiveDataAndErrorSerialization() {
    createReceiver()
        .then(waitForReceiveError)
        .then(receiveAndCheckData('a'))
        .then(serializeRoundTrip)
        .then(receiveAndCheckError(1))
        .then(receiveAndCheckData('b'))
        .then(receiveAndCheckError(3))
        .then(closeReceiver)
        .then(test.succeed, test.fail);
  },

  function testSerializeDuringReceive() {
    var receiver = createReceiver();
    Promise.all([
        receiver.then(receiveAndCheckError(FATAL_ERROR)),
        receiver
            .then(serializeRoundTrip)
            .then(receiveAndCheckData('a'))
            .then(closeReceiver)
    ]).then(test.succeed, test.fail);
  },

  function testSerializeAfterClose() {
    function receiveAfterClose(receiver) {
      test.assertThrows(
          receiver.receive, receiver, [], 'DataReceiver has been closed');
    }

    createReceiver()
        .then(closeReceiver)
        .then(serializeRoundTrip)
        .then(receiveAfterClose)
        .then(test.succeed, test.fail);
  },

  function testSourceShutdown() {
    createReceiver()
        .then(receiveAndCheckError(FATAL_ERROR))
        .then(closeReceiver)
        .then(test.succeed, test.fail);
  },

], test.runTests, exports);
