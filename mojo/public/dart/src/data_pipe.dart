// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of core;


class _MojoDataPipeNatives {
  static List MojoCreateDataPipe(
      int element_bytes, int capacity_bytes, int flags)
      native "MojoDataPipe_Create";

  static List MojoWriteData(int handle, ByteData data, int num_bytes, int flags)
      native "MojoDataPipe_WriteData";

  static List MojoBeginWriteData(int handle, int buffer_bytes, int flags)
      native "MojoDataPipe_BeginWriteData";

  static int MojoEndWriteData(int handle, int bytes_written)
      native "MojoDataPipe_EndWriteData";

  static List MojoReadData(int handle, ByteData data, int num_bytes, int flags)
      native "MojoDataPipe_ReadData";

  static List MojoBeginReadData(int handle, int buffer_bytes, int flags)
      native "MojoDataPipe_BeginReadData";

  static int MojoEndReadData(int handle, int bytes_read)
      native "MojoDataPipe_EndReadData";
}


class MojoDataPipeProducer {
  static const int FLAG_NONE = 0;
  static const int FLAG_ALL_OR_NONE = 1 << 0;

  MojoHandle handle;
  MojoResult status;
  final int element_bytes;

  MojoDataPipeProducer(this.handle,
                       this.status,
                       this.element_bytes);

  int write(ByteData data, [int num_bytes = -1, int flags = 0]) {
    if (handle == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return status;
    }

    int data_num_bytes = (num_bytes == -1) ? data.lengthInBytes : num_bytes;
    List result = _MojoDataPipeNatives.MojoWriteData(
        handle.h, data, data_num_bytes, flags);
    if (result == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return status;
    }

    assert((result is List) && (result.length == 2));
    status = new MojoResult(result[0]);
    return result[1];
  }

  ByteData beginWrite(int buffer_bytes, [int flags = 0]) {
    if (handle == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return null;
    }

    List result = _MojoDataPipeNatives.MojoBeginWriteData(
        handle.h, buffer_bytes, flags);
    if (result == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return null;
    }

    assert((result is List) && (result.length == 2));
    status = new MojoResult(result[0]);
    return result[1];
  }

  MojoResult endWrite(int bytes_written) {
    if (handle == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return status;
    }
    int result = _MojoDataPipeNatives.MojoEndWriteData(handle.h, bytes_written);
    status = new MojoResult(result);
    return status;
  }
}


class MojoDataPipeConsumer {
  static const int FLAG_NONE = 0;
  static const int FLAG_ALL_OR_NONE = 1 << 0;
  static const int FLAG_MAY_DISCARD = 1 << 1;
  static const int FLAG_QUERY = 1 << 2;
  static const int FLAG_PEEK = 1 << 3;

  MojoHandle handle;
  MojoResult status;
  final int element_bytes;

  MojoDataPipeConsumer(
      this.handle, [this.status = MojoResult.OK, this.element_bytes = 1]);

  int read(ByteData data, [int num_bytes = -1, int flags = 0]) {
    if (handle == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return status;
    }

    int data_num_bytes = (num_bytes == -1) ? data.lengthInBytes : num_bytes;
    List result = _MojoDataPipeNatives.MojoReadData(
        handle.h, data, data_num_bytes, flags);
    if (result == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return status;
    }
    assert((result is List) && (result.length == 2));
    status = new MojoResult(result[0]);
    return result[1];
  }

  ByteData beginRead([int buffer_bytes = 0, int flags = 0]) {
    if (handle == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return null;
    }

    List result = _MojoDataPipeNatives.MojoBeginReadData(
        handle.h, buffer_bytes, flags);
    if (result == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return null;
    }

    assert((result is List) && (result.length == 2));
    status = new MojoResult(result[0]);
    return result[1];
  }

  MojoResult endRead(int bytes_read) {
    if (handle == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return status;
    }
    int result = _MojoDataPipeNatives.MojoEndReadData(handle.h, bytes_read);
    status = new MojoResult(result);
    return status;
  }

  int query() => read(null, 0, FLAG_QUERY);
}


class MojoDataPipe {
  static const int FLAG_NONE = 0;
  static const int FLAG_MAY_DISCARD = 1 << 0;
  static const int DEFAULT_ELEMENT_SIZE = 1;
  static const int DEFAULT_CAPACITY = 0;

  MojoDataPipeProducer producer;
  MojoDataPipeConsumer consumer;
  MojoResult status;

  MojoDataPipe._internal() {
    producer = null;
    consumer = null;
    status = MojoResult.OK;
  }

  factory MojoDataPipe([int element_bytes = DEFAULT_ELEMENT_SIZE,
                        int capacity_bytes = DEFAULT_CAPACITY,
                        int flags = FLAG_NONE]) {
    List result = _MojoDataPipeNatives.MojoCreateDataPipe(
        element_bytes, capacity_bytes, flags);
    if (result == null) {
      return null;
    }
    assert((result is List) && (result.length == 3));
    MojoHandle producer_handle = new MojoHandle(result[1]);
    MojoHandle consumer_handle = new MojoHandle(result[2]);
    MojoDataPipe pipe = new MojoDataPipe._internal();
    pipe.producer = new MojoDataPipeProducer(
        producer_handle, new MojoResult(result[0]), element_bytes);
    pipe.consumer = new MojoDataPipeConsumer(
        consumer_handle, new MojoResult(result[0]), element_bytes);
    pipe.status = new MojoResult(result[0]);
    return pipe;
  }
}
