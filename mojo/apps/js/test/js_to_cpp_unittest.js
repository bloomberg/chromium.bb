// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/apps/js/test/js_to_cpp_unittest", [
    "mojo/apps/js/test/js_to_cpp.mojom",
    "mojo/public/js/bindings/connection",
    "mojo/public/js/bindings/connector",
    "mojo/public/js/bindings/core",
], function (jsToCpp, connection, connector, core) {
  var retainedConnection;
  var BAD_VALUE = 13;
  var DATA_PIPE_PARAMS = {
    flags: core.CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,
    elementNumBytes: 1,
    capacityNumBytes: 64
  };

  function JsSideConnection(cppSide) {
    this.cppSide_ = cppSide;
    cppSide.startTest();
  }

  JsSideConnection.prototype = Object.create(jsToCpp.JsSideStub.prototype);

  JsSideConnection.prototype.ping = function (arg) {
    this.cppSide_.pingResponse();
  };

  JsSideConnection.prototype.echo = function (numIterations, arg) {
    var arg2;
    var data_pipe1;
    var data_pipe2;
    var i;
    var message_pipe1;
    var message_pipe2;

    // Ensure expected negative values are negative.
    if (arg.si64 > 0)
      arg.si64 = BAD_VALUE;

    if (arg.si32 > 0)
      arg.si32 = BAD_VALUE;

    if (arg.si16 > 0)
      arg.si16 = BAD_VALUE;

    if (arg.si8 > 0)
      arg.si8 = BAD_VALUE;

    for (i = 0; i < numIterations; ++i) {
      data_pipe1 = core.createDataPipe(DATA_PIPE_PARAMS);
      data_pipe2 = core.createDataPipe(DATA_PIPE_PARAMS);
      message_pipe1 = core.createMessagePipe();
      message_pipe2 = core.createMessagePipe();

      arg.data_handle = data_pipe1.consumerHandle;
      arg.message_handle = message_pipe1.handle1;

      arg2 = new jsToCpp.EchoArgs();
      arg2.si64 = -1;
      arg2.si32 = -1;
      arg2.si16 = -1;
      arg2.si8 = -1;
      arg2.name = "going";
      arg2.data_handle = data_pipe2.consumerHandle;
      arg2.message_handle = message_pipe2.handle1;

      this.cppSide_.echoResponse(arg, arg2);

      core.close(data_pipe1.producerHandle);
      core.close(data_pipe2.producerHandle);
      core.close(message_pipe1.handle0);
      core.close(message_pipe2.handle0);
    }
    this.cppSide_.testFinished();
  };

  JsSideConnection.prototype.bitFlip = function (arg) {
    var iteration = 0;
    var data_pipe;
    var message_pipe;
    var stopSignalled = false;
    var proto = connector.Connector.prototype;
    proto.realAccept = proto.accept;
    proto.accept = function (message) {
      var offset = iteration / 8;
      var mask;
      var value;
      if (offset < message.buffer.arrayBuffer.byteLength) {
        mask = 1 << (iteration % 8);
        value = message.buffer.dataView.getUint8(offset) ^ mask;
        message.buffer.dataView.setUint8(offset, value);
        return this.realAccept(message);
      }
      stopSignalled = true;
      return false;
    };
    while (!stopSignalled) {
      data_pipe = core.createDataPipe(DATA_PIPE_PARAMS);
      message_pipe = core.createMessagePipe();
      arg.data_handle = data_pipe.consumerHandle;
      arg.message_handle = message_pipe.handle1;
      this.cppSide_.bitFlipResponse(arg);
      core.close(data_pipe.producerHandle);
      core.close(message_pipe.handle0);
      iteration += 1;
    }
    proto.accept = proto.realAccept;
    proto.realAccept = null;
    this.cppSide_.testFinished();
  };

  return function(handle) {
    retainedConnection = new connection.Connection(handle, JsSideConnection,
                                                   jsToCpp.CppSideProxy);
  };
});
