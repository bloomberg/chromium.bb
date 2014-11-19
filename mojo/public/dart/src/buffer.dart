// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of core;

class _MojoSharedBufferNatives {
  static List Create(int num_bytes, int flags)
      native "MojoSharedBuffer_Create";

  static List Duplicate(int buffer_handle, int flags)
      native "MojoSharedBuffer_Duplicate";

  static List Map(int buffer_handle, int offset, int num_bytes, int flags)
      native "MojoSharedBuffer_Map";

  static int Unmap(ByteData buffer)
      native "MojoSharedBuffer_Unmap";
}


class MojoSharedBuffer {
  static const int CREATE_FLAG_NONE = 0;
  static const int DUPLICATE_FLAG_NONE = 0;
  static const int MAP_FLAG_NONE = 0;

  RawMojoHandle handle;
  MojoResult status;
  ByteData mapping;

  MojoSharedBuffer._() {
    handle = null;
    status = MojoResult.OK;
    mapping = null;
  }

  factory MojoSharedBuffer(int num_bytes, [int flags = 0]) {
    List result = _MojoSharedBufferNatives.Create(num_bytes, flags);
    if (result == null) {
      return null;
    }
    assert((result is List) && (result.length == 2));
    var r = new MojoResult(result[0]);
    if (!r.isOk) {
      return null;
    }

    MojoSharedBuffer buf = new MojoSharedBuffer._();
    buf.status = r;
    buf.handle = new RawMojoHandle(result[1]);
    buf.mapping = null;
    return buf;
  }

  factory MojoSharedBuffer.duplicate(MojoSharedBuffer msb, [int flags = 0]) {
    List result = _MojoSharedBufferNatives.Duplicate(msb.handle.h, flags);
    if (result == null) {
      return null;
    }
    assert((result is List) && (result.length == 2));
    var r = new MojoResult(result[0]);
    if(!r.isOk) {
      return null;
    }

    MojoSharedBuffer dupe = new MojoSharedBuffer._();
    dupe.status = r;
    dupe.handle = new RawMojoHandle(result[1]);
    dupe.mapping = msb.mapping;
    return dupe;
  }

  MojoResult close() {
    if (handle == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return status;
    }
    MojoResult r = handle.close();
    status = r;
    mapping = null;
    return status;
  }

  MojoResult map(int offset, int num_bytes, [int flags = 0]) {
    if (handle == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return status;
    }
    List result = _MojoSharedBufferNatives.Map(
        handle.h, offset, num_bytes, flags);
    if (result == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return status;
    }
    assert((result is List) && (result.length == 2));
    status = new MojoResult(result[0]);
    mapping = result[1];
    return status;
  }

  MojoResult unmap() {
    int r = _MojoSharedBufferNatives.Unmap(mapping);
    status = new MojoResult(r);
    mapping = null;
    return status;
  }
}
