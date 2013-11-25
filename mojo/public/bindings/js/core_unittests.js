// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "gin/test/expect",
    "mojo/public/bindings/js/core",
  ], function(expect, core) {
  runWithPipe(testNop);
  runWithPipe(testReadAndWriteMessage);
  this.result = "PASS";

  function runWithPipe(test) {
    var pipe = core.createMessagePipe();

    test(pipe);

    expect(core.close(pipe.handle0)).toBe(core.RESULT_OK);
    expect(core.close(pipe.handle1)).toBe(core.RESULT_OK);
  }

  function testNop(pipe) {
  }

  function testReadAndWriteMessage(pipe) {
    var senderData = new Uint8Array(42);
    for (var i = 0; i < senderData.length; ++i) {
      senderData[i] = i * i;
    }

    var result = core.writeMessage(
      pipe.handle0, senderData, [],
      core.WRITE_MESSAGE_FLAG_NONE);

    expect(result).toBe(core.RESULT_OK);

    var receiverData = new Uint8Array(50);

    var read = core.readMessage(
      pipe.handle1, core.READ_MESSAGE_FLAG_NONE)

    expect(read.result).toBe(core.RESULT_OK);
    expect(read.buffer.byteLength).toBe(42);
    expect(read.handles.length).toBe(0);

    var memory = new Uint8Array(read.buffer);
    for (var i = 0; i < memory.length; ++i)
      expect(memory[i]).toBe((i * i) & 0xFF);
  }
});
