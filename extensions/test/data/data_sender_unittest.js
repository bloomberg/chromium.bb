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

function createSender() {
  return Promise.all([
    requireAsync('content/public/renderer/service_provider'),
    requireAsync('data_sender'),
    requireAsync('device/serial/data_stream.mojom'),
  ]).then(test.callbackPass(function(modules) {
    var serviceProvider = modules[0];
    var dataSender = modules[1];
    var dataStream = modules[2];
    return new dataSender.DataSender(
        serviceProvider.connectToService(dataStream.DataSinkProxy.NAME_),
        BUFFER_SIZE,
        FATAL_ERROR);
  }));
}

unittestBindings.exportTests([
  function testSend() {
    createSender().then(test.callbackPass(function(sender) {
      var seen = null;
      sender.send(generateData(1)).then(test.callbackPass(function(bytesSent) {
        test.assertEq(1, bytesSent);
        test.assertEq(null, seen);
        seen = 'first';
      }));
      sender.send(generateData(1)).then(test.callbackPass(function(bytesSent) {
        sender.close();
        test.assertEq(1, bytesSent);
        test.assertEq('first', seen);
        seen = 'second';
      }));
    }));
  },

  function testLargeSend() {
    createSender().then(test.callbackPass(function(sender) {
      sender.send(generateData(BUFFER_SIZE * 100, '1234567890')).then(
          test.callbackPass(function(bytesSent) {
        test.assertEq(BUFFER_SIZE * 100, bytesSent);
        sender.close();
      }));
    }));
  },

  function testSendError() {
    createSender().then(test.callbackPass(function(sender) {
      sender.send(generateData(BUFFER_SIZE * 100, 'b')).catch(test.callbackPass(
          function(e) {
        test.assertEq(1, e.error);
        test.assertEq(0, e.bytesSent);
        sender.send(generateData(1)).then(test.callbackPass(
            function(bytesSent) {
          test.assertEq(1, bytesSent);
          sender.close();
        }));
      }));
    }));
  },

  function testSendErrorPartialSuccess() {
    createSender().then(test.callbackPass(function(sender) {
      sender.send(generateData(BUFFER_SIZE * 100, 'b')).catch(test.callbackPass(
          function(e) {
        test.assertEq(1, e.error);
        test.assertEq(5, e.bytesSent);
        sender.send(generateData(1)).then(test.callbackPass(
            function(bytesSent) {
          test.assertEq(1, bytesSent);
          sender.close();
        }));
      }));
    }));
  },

  function testSendErrorBetweenPackets() {
    createSender().then(test.callbackPass(function(sender) {
      sender.send(generateData(2, 'b')).catch(test.callbackPass(function(e) {
        test.assertEq(1, e.error);
        test.assertEq(2, e.bytesSent);
      }));
      // After an error, all sends in progress will be cancelled.
      sender.send(generateData(2, 'b')).catch(test.callbackPass(function(e) {
        test.assertEq(1, e.error);
        test.assertEq(0, e.bytesSent);
        sender.send(generateData(1)).then(test.callbackPass(
            function(bytesSent) {
          test.assertEq(1, bytesSent);
          sender.close();
        }));
      }));
    }));
  },

  function testSendErrorInSecondPacket() {
    createSender().then(test.callbackPass(function(sender) {
      sender.send(generateData(2, 'b')).then(test.callbackPass(
          function(bytesSent) {
        test.assertEq(2, bytesSent);
      }));
      sender.send(generateData(2, 'b')).catch(test.callbackPass(function(e) {
        test.assertEq(1, e.error);
        test.assertEq(1, e.bytesSent);
        sender.send(generateData(1)).then(test.callbackPass(
            function(bytesSent) {
          test.assertEq(1, bytesSent);
          sender.close();
        }));
      }));
    }));
  },

  function testSendErrorInLargeSend() {
    createSender().then(test.callbackPass(function(sender) {
      sender.send(generateData(BUFFER_SIZE * 100, '1234567890')).catch(
          test.callbackPass(function(e) {
        test.assertEq(1, e.error);
        test.assertEq(12, e.bytesSent);
        sender.send(generateData(1)).then(test.callbackPass(
            function(bytesSent) {
          test.assertEq(1, bytesSent);
          sender.close();
        }));
      }));
    }));
  },

  function testSendErrorBeforeLargeSend() {
    createSender().then(test.callbackPass(function(sender) {
      sender.send(generateData(5, 'b')).catch(test.callbackPass(function(e) {
        test.assertEq(1, e.error);
        test.assertEq(2, e.bytesSent);
      }));
      sender.send(generateData(BUFFER_SIZE * 100, '1234567890')).catch(
          test.callbackPass(function(e) {
        test.assertEq(1, e.error);
        test.assertEq(0, e.bytesSent);
        sender.send(generateData(1)).then(test.callbackPass(
            function(bytesSent) {
          test.assertEq(1, bytesSent);
          sender.close();
        }));
      }));
    }));
  },

  function testCancelWithoutSend() {
    createSender().then(test.callbackPass(function(sender) {
      sender.cancel(3).then(test.callbackPass(function() {
        sender.close();
      }));
    }));
  },

  function testCancel() {
    createSender().then(test.callbackPass(function(sender) {
      var seen = null;
      sender.send(generateData(1, 'b')).catch(test.callbackPass(function(e) {
        test.assertEq(3, e.error);
        test.assertEq(0, e.bytesSent);
        test.assertEq(null, seen);
        seen = 'send';
      }));
      sender.cancel(3).then(test.callbackPass(function() {
        test.assertEq('send', seen);
        seen = 'cancel';
        sender.close();
      }));
      test.assertThrows(
          sender.cancel, sender, [], 'Cancel already in progress');
      test.assertThrows(sender.send, sender, [], 'Cancel in progress');
    }));
  },

  function testClose() {
    createSender().then(test.callbackPass(function(sender) {
      var seen = null;
      sender.send(generateData(1, 'b')).catch(test.callbackPass(function(e) {
        test.assertEq(FATAL_ERROR, e.error);
        test.assertEq(0, e.bytesSent);
        test.assertEq(null, seen);
        seen = 'send';
      }));
      sender.cancel(3).then(test.callbackPass(function() {
        test.assertEq('send', seen);
        seen = 'cancel';
        sender.close();
      }));
      sender.close();
      test.assertThrows(
          sender.cancel, sender, [], 'DataSender has been closed');
      test.assertThrows(sender.send, sender, [], 'DataSender has been closed');
    }));
  },

], test.runTests, exports);
