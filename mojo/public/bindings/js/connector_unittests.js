// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mock out the support module to avoid depending on the message loop.
define("mojo/public/bindings/js/support", function() {
  var waitingCallbacks = [];

  function WaitCookie(id) {
    this.id = id;
  }

  function asyncWait(handle, flags, callback) {
    var id = waitingCallbacks.length;
    waitingCallbacks.push(callback);
    return new WaitCookie(id);
  }

  function cancelWait(cookie) {
    waitingCallbacks[cookie.id] = null;
  }

  function numberOfWaitingCallbacks() {
    var count = 0;
    for (var i = 0; i < waitingCallbacks.length; ++i) {
      if (waitingCallbacks[i])
        ++count;
    }
    return count;
  }

  function pumpOnce(result) {
    var callbacks = waitingCallbacks;
    waitingCallbacks = [];
    for (var i = 0; i < callbacks.length; ++i)
      callbacks[i](result);
  }

  var exports = {};
  exports.asyncWait = asyncWait;
  exports.cancelWait = cancelWait;
  exports.numberOfWaitingCallbacks = numberOfWaitingCallbacks;
  exports.pumpOnce = pumpOnce;
  return exports;
});

define([
    "gin/test/expect",
    "mojo/public/bindings/js/support",
    "mojo/public/bindings/js/core",
    "mojo/public/bindings/js/connector",
    "mojom/sample_service",
], function(expect, mockSupport, core, connector, sample) {

  var receivedFrobinate = false;
  var receivedDidFrobinate = false;

  // ServiceImpl --------------------------------------------------------------

  function ServiceImpl(peer) {
    this.peer = peer;
  }

  ServiceImpl.prototype = Object.create(sample.ServiceStub.prototype);

  ServiceImpl.prototype.frobinate = function(foo, baz, port) {
    receivedFrobinate = true;

    expect(foo.name).toBe("Example name");
    expect(baz).toBeTruthy();
    expect(core.close(port)).toBe(core.RESULT_OK);

    this.peer.didFrobinate(42);
  };

  // ServiceImpl --------------------------------------------------------------

  function ServiceClientImpl(peer) {
    this.peer = peer;
  }

  ServiceClientImpl.prototype =
      Object.create(sample.ServiceClientStub.prototype);

  ServiceClientImpl.prototype.didFrobinate = function(result) {
    receivedDidFrobinate = true;

    expect(result).toBe(42);
  };

  var pipe = core.createMessagePipe();
  var anotherPipe = core.createMessagePipe();

  var connection0 = new connector.Connection(
      pipe.handle0, ServiceImpl, sample.ServiceClientProxy);

  var connection1 = new connector.Connection(
      pipe.handle1, ServiceClientImpl, sample.ServiceProxy);

  var foo = new sample.Foo();
  foo.bar = new sample.Bar();
  foo.name = "Example name";
  connection1.remote.frobinate(foo, true, anotherPipe.handle0);

  mockSupport.pumpOnce(core.RESULT_OK);

  expect(receivedFrobinate).toBeTruthy();
  expect(receivedDidFrobinate).toBeTruthy();

  connection0.close();
  connection1.close();

  expect(mockSupport.numberOfWaitingCallbacks()).toBe(0);

  // anotherPipe.handle0 was closed automatically when sent over IPC.
  expect(core.close(anotherPipe.handle0)).toBe(core.RESULT_INVALID_ARGUMENT);
  // anotherPipe.handle1 hasn't been closed yet.
  expect(core.close(anotherPipe.handle1)).toBe(core.RESULT_OK);

  // The Connection object is responsible for closing these handles.
  expect(core.close(pipe.handle0)).toBe(core.RESULT_INVALID_ARGUMENT);
  expect(core.close(pipe.handle1)).toBe(core.RESULT_INVALID_ARGUMENT);

  this.result = "PASS";
});
