// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "gin/test/expect",
    "mojo/public/js/core",
    "gc",
  ], function(expect, core, gc) {

  var HANDLE_SIGNAL_READWRITABLE = core.HANDLE_SIGNAL_WRITABLE |
                                   core.HANDLE_SIGNAL_READABLE;
  var HANDLE_SIGNAL_ALL = core.HANDLE_SIGNAL_WRITABLE |
                          core.HANDLE_SIGNAL_READABLE |
                          core.HANDLE_SIGNAL_PEER_CLOSED |
                          core.HANDLE_SIGNAL_PEER_REMOTE;

  runWithMessagePipe(testNop);
  runWithMessagePipe(testReadAndWriteMessage);
  runWithMessagePipeWithOptions(testNop);
  runWithMessagePipeWithOptions(testReadAndWriteMessage);
  runWithDataPipe(testNop);
  runWithDataPipe(testReadAndWriteDataPipe);
  runWithDataPipeWithOptions(testNop);
  runWithDataPipeWithOptions(testReadAndWriteDataPipe);
  runWithMessagePipe(testIsHandleMessagePipe);
  runWithDataPipe(testIsHandleDataPipe);
  runWithSharedBuffer(testSharedBuffer);
  gc.collectGarbage();  // should not crash
  this.result = "PASS";

  function runWithMessagePipe(test) {
    var pipe = core.createMessagePipe();
    expect(pipe.result).toBe(core.RESULT_OK);

    test(pipe);

    expect(core.close(pipe.handle0)).toBe(core.RESULT_OK);
    expect(core.close(pipe.handle1)).toBe(core.RESULT_OK);
  }

  function runWithMessagePipeWithOptions(test) {
    var pipe = core.createMessagePipe({
        flags: core.CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE
    });
    expect(pipe.result).toBe(core.RESULT_OK);

    test(pipe);

    expect(core.close(pipe.handle0)).toBe(core.RESULT_OK);
    expect(core.close(pipe.handle1)).toBe(core.RESULT_OK);
  }

  function runWithDataPipe(test) {
    var pipe = core.createDataPipe();
    expect(pipe.result).toBe(core.RESULT_OK);

    test(pipe);

    expect(core.close(pipe.producerHandle)).toBe(core.RESULT_OK);
    expect(core.close(pipe.consumerHandle)).toBe(core.RESULT_OK);
  }

  function runWithDataPipeWithOptions(test) {
    var pipe = core.createDataPipe({
        flags: core.CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,
        elementNumBytes: 1,
        capacityNumBytes: 64
        });
    expect(pipe.result).toBe(core.RESULT_OK);

    test(pipe);

    expect(core.close(pipe.producerHandle)).toBe(core.RESULT_OK);
    expect(core.close(pipe.consumerHandle)).toBe(core.RESULT_OK);
  }

  function runWithSharedBuffer(test) {
    let buffer_size = 32;
    let sharedBuffer = core.createSharedBuffer(buffer_size,
        core.CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE);

    expect(sharedBuffer.result).toBe(core.RESULT_OK);
    expect(core.isHandle(sharedBuffer.handle)).toBeTruthy();

    test(sharedBuffer, buffer_size);
  }

  function testNop(pipe) {
  }

  function testReadAndWriteMessage(pipe) {
    var state0 = core.queryHandleSignalsState(pipe.handle0);
    expect(state0.result).toBe(core.RESULT_OK);
    expect(state0.satisfiedSignals).toBe(core.HANDLE_SIGNAL_WRITABLE);
    expect(state0.satisfiableSignals).toBe(HANDLE_SIGNAL_ALL);

    var state1 = core.queryHandleSignalsState(pipe.handle1);
    expect(state1.result).toBe(core.RESULT_OK);
    expect(state1.satisfiedSignals).toBe(core.HANDLE_SIGNAL_WRITABLE);
    expect(state1.satisfiableSignals).toBe(HANDLE_SIGNAL_ALL);

    var senderData = new Uint8Array(42);
    for (var i = 0; i < senderData.length; ++i) {
      senderData[i] = i * i;
    }

    var result = core.writeMessage(
      pipe.handle0, senderData, [],
      core.WRITE_MESSAGE_FLAG_NONE);

    expect(result).toBe(core.RESULT_OK);

    state0 = core.queryHandleSignalsState(pipe.handle0);
    expect(state0.result).toBe(core.RESULT_OK);
    expect(state0.satisfiedSignals).toBe(core.HANDLE_SIGNAL_WRITABLE);
    expect(state0.satisfiableSignals).toBe(HANDLE_SIGNAL_ALL);

    var wait = core.wait(pipe.handle1, core.HANDLE_SIGNAL_READABLE);
    expect(wait.result).toBe(core.RESULT_OK);
    expect(wait.signalsState.satisfiedSignals).toBe(HANDLE_SIGNAL_READWRITABLE);
    expect(wait.signalsState.satisfiableSignals).toBe(HANDLE_SIGNAL_ALL);

    var read = core.readMessage(pipe.handle1, core.READ_MESSAGE_FLAG_NONE);

    expect(read.result).toBe(core.RESULT_OK);
    expect(read.buffer.byteLength).toBe(42);
    expect(read.handles.length).toBe(0);

    var memory = new Uint8Array(read.buffer);
    for (var i = 0; i < memory.length; ++i)
      expect(memory[i]).toBe((i * i) & 0xFF);
  }

  function testReadAndWriteDataPipe(pipe) {
    var senderData = new Uint8Array(42);
    for (var i = 0; i < senderData.length; ++i) {
      senderData[i] = i * i;
    }

    var write = core.writeData(
      pipe.producerHandle, senderData,
      core.WRITE_DATA_FLAG_ALL_OR_NONE);

    expect(write.result).toBe(core.RESULT_OK);
    expect(write.numBytes).toBe(42);

    var wait = core.wait(pipe.consumerHandle, core.HANDLE_SIGNAL_READABLE);
    expect(wait.result).toBe(core.RESULT_OK);
    var peeked = core.readData(
         pipe.consumerHandle,
         core.READ_DATA_FLAG_PEEK | core.READ_DATA_FLAG_ALL_OR_NONE);
    expect(peeked.result).toBe(core.RESULT_OK);
    expect(peeked.buffer.byteLength).toBe(42);

    var peeked_memory = new Uint8Array(peeked.buffer);
    for (var i = 0; i < peeked_memory.length; ++i)
      expect(peeked_memory[i]).toBe((i * i) & 0xFF);

    var read = core.readData(
      pipe.consumerHandle, core.READ_DATA_FLAG_ALL_OR_NONE);

    expect(read.result).toBe(core.RESULT_OK);
    expect(read.buffer.byteLength).toBe(42);

    var memory = new Uint8Array(read.buffer);
    for (var i = 0; i < memory.length; ++i)
      expect(memory[i]).toBe((i * i) & 0xFF);
  }

  function testIsHandleMessagePipe(pipe) {
    expect(core.isHandle(123).toBeFalsy);
    expect(core.isHandle("123").toBeFalsy);
    expect(core.isHandle({}).toBeFalsy);
    expect(core.isHandle([]).toBeFalsy);
    expect(core.isHandle(undefined).toBeFalsy);
    expect(core.isHandle(pipe).toBeFalsy);
    expect(core.isHandle(pipe.handle0)).toBeTruthy();
    expect(core.isHandle(pipe.handle1)).toBeTruthy();
    expect(core.isHandle(null)).toBeTruthy();
  }

  function testIsHandleDataPipe(pipe) {
    expect(core.isHandle(pipe.consumerHandle)).toBeTruthy();
    expect(core.isHandle(pipe.producerHandle)).toBeTruthy();
  }

  function testSharedBuffer(sharedBuffer, buffer_size) {
    let offset = 0;
    let mappedBuffer0 = core.mapBuffer(sharedBuffer.handle,
                                       offset,
                                       buffer_size,
                                       core.MAP_BUFFER_FLAG_NONE);

    expect(mappedBuffer0.result).toBe(core.RESULT_OK);

    let dupedBufferHandle = core.duplicateBufferHandle(sharedBuffer.handle,
        core.DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE);

    expect(dupedBufferHandle.result).toBe(core.RESULT_OK);
    expect(core.isHandle(dupedBufferHandle.handle)).toBeTruthy();

    let mappedBuffer1 = core.mapBuffer(dupedBufferHandle.handle,
                                       offset,
                                       buffer_size,
                                       core.MAP_BUFFER_FLAG_NONE);

    expect(mappedBuffer1.result).toBe(core.RESULT_OK);

    let buffer0 = new Uint8Array(mappedBuffer0.buffer);
    let buffer1 = new Uint8Array(mappedBuffer1.buffer);
    for(let i = 0; i < buffer0.length; ++i) {
      buffer0[i] = i;
      expect(buffer1[i]).toBe(i);
    }

    expect(core.unmapBuffer(mappedBuffer0.buffer)).toBe(core.RESULT_OK);
    expect(core.unmapBuffer(mappedBuffer1.buffer)).toBe(core.RESULT_OK);

    expect(core.close(dupedBufferHandle.handle)).toBe(core.RESULT_OK);
    expect(core.close(sharedBuffer.handle)).toBe(core.RESULT_OK);
  }

});
