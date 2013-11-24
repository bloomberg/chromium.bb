// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "gtest",
    "mojo/public/bindings/js/core",
  ], function(gtest, core) {
  runWithPipe(testNop);
  runWithPipe(testReadAndWriteMessage);
  this.result = "PASS";

  function runWithPipe(test) {
    var pipe = core.createMessagePipe();

    test(pipe);

    var result0 = core.close(pipe.handle0);
    gtest.expectEqual(result0, core.RESULT_OK,
        "result0 is " + result0);

    var result1 = core.close(pipe.handle1);
    gtest.expectEqual(result1, core.RESULT_OK,
        "result1 is " + result1);
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

    gtest.expectEqual(result, core.RESULT_OK,
      "writeMessage returned RESULT_OK: " + result);

    var receiverData = new Uint8Array(50);

    var read = core.readMessage(
      pipe.handle1, core.READ_MESSAGE_FLAG_NONE)

    gtest.expectEqual(read.result, core.RESULT_OK,
        "read.result is " + read.result);
    gtest.expectEqual(read.buffer.byteLength, 42,
        "read.buffer.byteLength is " + read.buffer.byteLength);
    gtest.expectEqual(read.handles.length, 0,
        "read.handles.length is " + read.handles.length);

    var memory = new Uint8Array(read.buffer);
    for (var i = 0; i < memory.length; ++i) {
      gtest.expectEqual(memory[i], (i * i) & 0xFF,
          "memory[" + i + "] is " + memory[i]);
    }
  }
});
