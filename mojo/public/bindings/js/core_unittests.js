// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function runWithPipe(mojo, test) {
  var pipe = mojo.core.createMessagePipe();

  test(mojo, pipe);

  var result0 = mojo.core.close(pipe.handle0);
  mojo.gtest.expectEqual(result0, mojo.core.RESULT_OK,
      "result0 is " + result0);

  var result1 = mojo.core.close(pipe.handle1);
  mojo.gtest.expectEqual(result1, mojo.core.RESULT_OK,
      "result1 is " + result1);
}

function testNop(mojo, pipe) {
}

function testReadAndWriteMessage(mojo, pipe) {
  var senderData = new Uint8Array(42);
  for (var i = 0; i < senderData.length; ++i) {
    senderData[i] = i * i;
  }

  var result = mojo.core.writeMessage(
    pipe.handle0, senderData, [],
    mojo.core.WRITE_MESSAGE_FLAG_NONE);

  mojo.gtest.expectEqual(result, mojo.core.RESULT_OK,
    "writeMessage returned RESULT_OK: " + result);

  var receiverData = new Uint8Array(50);

  var mesage = mojo.core.readMessage(
    pipe.handle1, receiverData, 10,
    mojo.core.READ_MESSAGE_FLAG_NONE)

  mojo.gtest.expectEqual(mesage.result, mojo.core.RESULT_OK,
      "mesage.result is " + mesage.result);
  mojo.gtest.expectEqual(mesage.bytesRead, 42,
      "mesage.bytesRead is " + mesage.bytesRead);
  mojo.gtest.expectEqual(mesage.handles.length, 0,
      "mesage.handles.length is " + mesage.handles.length);

  for (var i = 0; i < mesage.bytesRead; ++i) {
    mojo.gtest.expectEqual(receiverData[i], (i * i) & 0xFF,
        "receiverData[" + i + "] is " + receiverData[i]);
  }
}

function main(mojo) {
  runWithPipe(mojo, testNop);
  runWithPipe(mojo, testReadAndWriteMessage);
}
