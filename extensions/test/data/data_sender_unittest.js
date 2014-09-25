// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests launched by extensions/renderer/api/serial/data_sender_unittest.cc

var test = require('test').binding;
var unittestBindings = require('test_environment_specific_bindings');

var BUFFER_SIZE = 11;
var FATAL_ERROR = 2;

function generateData(size, pattern) {
  if (!pattern)
    pattern = 'a';
  var buffer = new ArrayBuffer(size);
  var intView = new Int8Array(buffer);
  for (var i = 0; i < size; i++) {
    intView[i] = pattern.charCodeAt(i % pattern.length);
  }
  return buffer;
}

// Returns a promise to a newly created DataSender.
function createSender() {
  return Promise.all([
    requireAsync('content/public/renderer/service_provider'),
    requireAsync('data_sender'),
    requireAsync('device/serial/data_stream.mojom'),
  ]).then(function(modules) {
    var serviceProvider = modules[0];
    var dataSender = modules[1];
    var dataStream = modules[2];
    return new dataSender.DataSender(
        serviceProvider.connectToService(dataStream.DataSinkProxy.NAME_),
        BUFFER_SIZE,
        FATAL_ERROR);
  });
}

// Returns a function that sends data to a provided DataSender |sender|,
// checks that the send completes successfully and returns a promise that will
// resolve to |sender|.
function sendAndExpectSuccess(data) {
  return function(sender) {
    return sender.send(data).then(function(bytesSent) {
      test.assertEq(data.byteLength, bytesSent);
      return sender;
    });
  };
}

// Returns a function that sends data to a provided DataSender |sender|,
// checks that the send fails with the expected error and expected number of
// bytes sent, and returns a promise that will resolve to |sender|.
function sendAndExpectError(data, expectedError, expectedBytesSent) {
  return function(sender) {
    return sender.send(data).catch(function(result) {
      test.assertEq(expectedError, result.error);
      test.assertEq(expectedBytesSent, result.bytesSent);
      return sender;
    });
  };
}

// Returns a function that cancels sends on the provided DataSender |sender|
// with error |cancelReason|, returning a promise that will resolve to |sender|
// once the cancel completes.
function cancelSend(cancelReason) {
  return function(sender) {
    return sender.cancel(cancelReason).then(function() {
      return sender;
    });
  };
}

// Checks that attempting to start a send with |sender| fails.
function sendAfterClose(sender) {
  test.assertThrows(sender.send, sender, [], 'DataSender has been closed');
}

// Checks that the provided promises resolve in order, returning the result of
// the first.
function expectOrder(promises) {
  var nextIndex = 0;
  function createOrderChecker(promise, expectedIndex) {
    return promise.then(function(sender) {
      test.assertEq(nextIndex, expectedIndex);
      nextIndex++;
      return sender;
    });
  }
  var wrappedPromises = [];
  for (var i = 0; i < promises.length; i++) {
    wrappedPromises.push(createOrderChecker(promises[i], i));
  }
  return Promise.all(wrappedPromises).then(function(results) {
    return results[0];
  });
}

// Serializes and deserializes the provided DataSender |sender|, returning a
// promise that will resolve to the newly deserialized DataSender.
function serializeRoundTrip(sender) {
  return Promise.all([
    sender.serialize(),
    requireAsync('data_sender'),
  ]).then(function(promises) {
    var serialized = promises[0];
    var dataSenderModule = promises[1];
    return dataSenderModule.DataSender.deserialize(serialized);
  });
}

function closeSender(sender) {
  sender.close();
  return sender;
}

unittestBindings.exportTests([
  function testSend() {
    var sender = createSender();
    expectOrder([
        sender.then(sendAndExpectSuccess(generateData(1))),
        sender.then(sendAndExpectSuccess(generateData(1))),
    ])
        .then(closeSender)
        .then(test.succeed, test.fail);
  },

  function testLargeSend() {
    createSender()
        .then(sendAndExpectSuccess(generateData(BUFFER_SIZE * 3, '123')))
        .then(closeSender)
        .then(test.succeed, test.fail);
  },

  function testSendError() {
    createSender()
        .then(sendAndExpectError(generateData(BUFFER_SIZE * 3, 'b'), 1, 0))
        .then(sendAndExpectSuccess(generateData(1)))
        .then(closeSender)
        .then(test.succeed, test.fail);
  },

  function testSendErrorPartialSuccess() {
    createSender()
        .then(sendAndExpectError(generateData(BUFFER_SIZE * 3, 'b'), 1, 5))
        .then(sendAndExpectSuccess(generateData(1)))
        .then(closeSender)
        .then(test.succeed, test.fail);
  },

  function testSendErrorBetweenPackets() {
    var sender = createSender();
    expectOrder([
        sender.then(sendAndExpectError(generateData(2, 'b'), 1, 2)),
        sender.then(sendAndExpectError(generateData(2, 'b'), 1, 0)),
    ])
        .then(sendAndExpectSuccess(generateData(1)))
        .then(closeSender)
        .then(test.succeed, test.fail);
  },

  function testSendErrorInSecondPacket() {
    var sender = createSender();
    expectOrder([
        sender.then(sendAndExpectSuccess(generateData(2, 'b'))),
        sender.then(sendAndExpectError(generateData(2, 'b'), 1, 1)),
    ])
        .then(sendAndExpectSuccess(generateData(1)))
        .then(closeSender)
        .then(test.succeed, test.fail);
  },

  function testSendErrorInLargeSend() {
    createSender()
        .then(sendAndExpectError(
            generateData(BUFFER_SIZE * 3, '1234567890'), 1, 12))
        .then(sendAndExpectSuccess(generateData(1)))
        .then(closeSender)
        .then(test.succeed, test.fail);
  },

  function testSendErrorBeforeLargeSend() {
    var sender = createSender();
    expectOrder([
        sender.then(sendAndExpectError(generateData(5, 'b'), 1, 2)),
        sender.then(sendAndExpectError(
            generateData(BUFFER_SIZE * 3, '1234567890'), 1, 0)),
    ])
        .then(sendAndExpectSuccess(generateData(1)))
        .then(closeSender)
        .then(test.succeed, test.fail);
  },

  function testCancelWithoutSend() {
    createSender()
        .then(cancelSend(3))
        .then(closeSender)
        .then(test.succeed, test.fail);
  },

  function testCancel() {
    var sender = createSender();
    expectOrder([
        sender.then(sendAndExpectError(generateData(1, 'b'), 3, 0)),
        sender.then(cancelSend(3)),
    ])
        .then(closeSender)
        .then(test.succeed, test.fail);
    sender.then(function(sender) {
      test.assertThrows(
          sender.cancel, sender, [], 'Cancel already in progress');
      test.assertThrows(sender.send, sender, [], 'Cancel in progress');
    });
  },

  function testClose() {
    var sender = createSender();
    expectOrder([
        sender.then(sendAndExpectError(generateData(1, 'b'), FATAL_ERROR, 0)),
        sender.then(cancelSend(3)),
    ]);
    sender
        .then(closeSender)
        .then(sendAfterClose)
        .then(test.succeed, test.fail);
  },

  function testSendAfterSerialization() {
    var sender = createSender().then(serializeRoundTrip);
    expectOrder([
        sender.then(sendAndExpectSuccess(generateData(1))),
        sender.then(sendAndExpectSuccess(generateData(1))),
    ])
        .then(closeSender)
        .then(test.succeed, test.fail);
  },

  function testSendErrorAfterSerialization() {
    createSender()
        .then(serializeRoundTrip)
        .then(sendAndExpectError(generateData(BUFFER_SIZE * 3, 'b'), 1, 0))
        .then(sendAndExpectSuccess(generateData(1)))
        .then(closeSender)
        .then(test.succeed, test.fail);
  },


  function testCancelAfterSerialization() {
    var sender = createSender().then(serializeRoundTrip);
    expectOrder([
        sender.then(sendAndExpectError(generateData(1, 'b'), 4, 0)),
        sender.then(cancelSend(4)),
    ])
        .then(closeSender)
        .then(test.succeed, test.fail);
  },

  function testSerializeCancelsSendsInProgress() {
    var sender = createSender();
    expectOrder([
        sender.then(sendAndExpectError(generateData(1, 'b'), FATAL_ERROR, 0)),
        sender.then(sendAndExpectError(generateData(1, 'b'), FATAL_ERROR, 0)),
        sender.then(serializeRoundTrip),
    ])
        .then(closeSender)
        .then(test.succeed, test.fail);
  },

  function testSerializeWaitsForCancel() {
    var sender = createSender();
    expectOrder([
        sender.then(sendAndExpectError(generateData(1, 'b'), 3, 0)),
        sender.then(cancelSend(3)),
        sender.then(serializeRoundTrip),
    ])
        .then(closeSender)
        .then(test.succeed, test.fail);
  },

  function testSerializeAfterClose() {
    createSender()
        .then(closeSender)
        .then(serializeRoundTrip)
        .then(sendAfterClose)
        .then(test.succeed, test.fail);
  },

], test.runTests, exports);
