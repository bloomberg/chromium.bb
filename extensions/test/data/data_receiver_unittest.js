// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests launched by extensions/renderer/api/serial/data_receiver_unittest.cc

var test = require('test').binding;
var unittestBindings = require('test_environment_specific_bindings');

var fatalErrorValue = 2;
var bufferSize = 10;

function createReceiver() {
  return Promise.all([
    requireAsync('content/public/renderer/service_provider'),
    requireAsync('data_receiver'),
    requireAsync('device/serial/data_stream.mojom'),
  ]).then(test.callbackPass(function(modules) {
    var serviceProvider = modules[0];
    var dataReceiver = modules[1];
    var dataStream = modules[2];
    return new dataReceiver.DataReceiver(
        serviceProvider.connectToService(dataStream.DataSourceProxy.NAME_),
        bufferSize,
        fatalErrorValue);
  }));
}

unittestBindings.exportTests([
  function testReceive() {
    createReceiver().then(test.callbackPass(function(receiver) {
      receiver.receive().then(test.callbackPass(function(data) {
        test.assertEq(1, data.byteLength);
        test.assertEq('a'.charCodeAt(0), new Int8Array(data)[0]);
        receiver.close();
      }));
      test.assertThrows(
          receiver.receive, receiver, [], 'Receive already in progress.');
    }));
  },

  function testReceiveError() {
    createReceiver().then(test.callbackPass(function(receiver) {
      receiver.receive().catch(test.callbackPass(function(error) {
        test.assertEq(1, error.error);
        receiver.close();
      }));
    }));
  },

  function testReceiveDataAndError() {
    createReceiver().then(test.callbackPass(function(receiver) {
      receiver.receive().then(test.callbackPass(function(data) {
        test.assertEq(1, data.byteLength);
        test.assertEq('a'.charCodeAt(0), new Int8Array(data)[0]);
        return receiver.receive();
      })).catch(test.callbackPass(function(error) {
        test.assertEq(1, error.error);
        return receiver.receive();
      })).then(test.callbackPass(function(data) {
        test.assertEq(1, data.byteLength);
        test.assertEq('b'.charCodeAt(0), new Int8Array(data)[0]);
        receiver.close();
      }));
    }));
  },

  function testReceiveErrorThenData() {
    createReceiver().then(test.callbackPass(function(receiver) {
      receiver.receive().catch(test.callbackPass(function(error) {
        test.assertEq(1, error.error);
        return receiver.receive();
      })).then(test.callbackPass(function(data) {
        test.assertEq(1, data.byteLength);
        test.assertEq('a'.charCodeAt(0), new Int8Array(data)[0]);
        receiver.close();
      }));
    }));
  },

  function testSourceShutdown() {
    createReceiver().then(test.callbackPass(function(receiver) {
      receiver.receive().catch(test.callbackPass(function(error) {
        test.assertEq(fatalErrorValue, error.error);
        receiver.close();
      }));
    }));
  },

], test.runTests, exports);
