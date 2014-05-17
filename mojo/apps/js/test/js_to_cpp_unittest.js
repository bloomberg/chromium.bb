// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('mojo/apps/js/test/js_to_cpp_unittest', [
  'console',
  'mojo/apps/js/test/js_to_cpp.mojom',
  'mojo/public/js/bindings/connection',
  'mojo/public/js/bindings/connector',
  'mojo/public/js/bindings/core',
], function (console, jsToCpp, connection, connector, core) {
  var retainedConnection;
  var senderData;
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

  JsSideConnection.prototype.echo = function (numIterations, list) {
    var arg = list.item;
    var dataPipe1;
    var dataPipe2;
    var i;
    var messagePipe1;
    var messagePipe2;
    var resultList;
    var specialArg;

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
      dataPipe1 = core.createDataPipe(DATA_PIPE_PARAMS);
      dataPipe2 = core.createDataPipe(DATA_PIPE_PARAMS);
      messagePipe1 = core.createMessagePipe();
      messagePipe2 = core.createMessagePipe();

      arg.data_handle = dataPipe1.consumerHandle;
      arg.message_handle = messagePipe1.handle1;

      specialArg = new jsToCpp.EchoArgs();
      specialArg.si64 = -1;
      specialArg.si32 = -1;
      specialArg.si16 = -1;
      specialArg.si8 = -1;
      specialArg.name = 'going';
      specialArg.data_handle = dataPipe2.consumerHandle;
      specialArg.message_handle = messagePipe2.handle1;

      writeDataPipe(dataPipe1, senderData);
      writeDataPipe(dataPipe2, senderData);

      resultList = new jsToCpp.EchoArgsList();
      resultList.next = new jsToCpp.EchoArgsList();
      resultList.item = specialArg;
      resultList.next.item = arg;

      this.cppSide_.echoResponse(resultList);

      core.close(dataPipe1.producerHandle);
      core.close(dataPipe2.producerHandle);
      core.close(messagePipe1.handle0);
      core.close(messagePipe2.handle0);
    }
    this.cppSide_.testFinished();
  };

  JsSideConnection.prototype.bitFlip = function (arg) {
    var iteration = 0;
    var dataPipe;
    var messagePipe;
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
      dataPipe = core.createDataPipe(DATA_PIPE_PARAMS);
      messagePipe = core.createMessagePipe();
      writeDataPipe(dataPipe, senderData);
      arg.data_handle = dataPipe.consumerHandle;
      arg.message_handle = messagePipe.handle1;
      this.cppSide_.bitFlipResponse(arg);
      core.close(dataPipe.producerHandle);
      core.close(messagePipe.handle0);
      iteration += 1;
    }

    proto.accept = proto.realAccept;
    proto.realAccept = null;
    this.cppSide_.testFinished();
  };

  function writeDataPipe(pipe, data) {
    var writeResult = core.writeData(
      pipe.producerHandle, data, core.WRITE_DATA_FLAG_ALL_OR_NONE);

    if (writeResult.result != core.RESULT_OK) {
      console.log('ERROR: Write result was ' + writeResult.result);
      return false;
    }
    if (writeResult.numBytes != data.length) {
      console.log('ERROR: Write length was ' + writeResult.numBytes);
      return false;
    }
    return true;
  }

  return function(handle) {
    var i;
    senderData = new Uint8Array(DATA_PIPE_PARAMS.capacityNumBytes);
    for (i = 0; i < senderData.length; ++i)
      senderData[i] = i;

    retainedConnection = new connection.Connection(handle, JsSideConnection,
                                                   jsToCpp.CppSideProxy);
  };
});
