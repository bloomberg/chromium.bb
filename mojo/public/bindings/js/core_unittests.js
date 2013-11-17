// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define(["gtest", "core"], function(gtest, core) {
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

    var mesage = core.readMessage(
      pipe.handle1, receiverData, 10,
      core.READ_MESSAGE_FLAG_NONE)

    gtest.expectEqual(mesage.result, core.RESULT_OK,
        "mesage.result is " + mesage.result);
    gtest.expectEqual(mesage.bytesRead, 42,
        "mesage.bytesRead is " + mesage.bytesRead);
    gtest.expectEqual(mesage.handles.length, 0,
        "mesage.handles.length is " + mesage.handles.length);

    for (var i = 0; i < mesage.bytesRead; ++i) {
      gtest.expectEqual(receiverData[i], (i * i) & 0xFF,
          "receiverData[" + i + "] is " + receiverData[i]);
    }
  }
});
