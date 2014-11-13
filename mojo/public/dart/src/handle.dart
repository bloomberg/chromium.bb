// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of core;


class _MojoHandleNatives {
  static int close(int handle) native "MojoHandle_Close";
  static int wait(int handle, int signals, int deadline)
      native "MojoHandle_Wait";
  static int waitMany(
      List handles, List signals, int num_handles, int deadline)
      native "MojoHandle_WaitMany";
}


class RawMojoHandle {
  static const int INVALID = 0;
  static const int DEADLINE_INDEFINITE = -1;

  RawMojoHandle(this.h);

  MojoResult close() {
    int result = _MojoHandleNatives.close(h);
    h = INVALID;
    return new MojoResult(result);
  }

  MojoResult wait(int signals, int deadline) {
    int result = _MojoHandleNatives.wait(h, signals, deadline);
    return new MojoResult(result);
  }

  bool _ready(int signal) {
    MojoResult res = wait(signal, 0);
    switch (res) {
      case MojoResult.OK:
        return true;
      case MojoResult.DEADLINE_EXCEEDED:
      case MojoResult.CANCELLED:
      case MojoResult.INVALID_ARGUMENT:
      case MojoResult.FAILED_PRECONDITION:
        return false;
      default:
        // Should be unreachable.
        throw new Exception("Unreachable");
    }
  }

  bool readyRead() => _ready(MojoHandleSignals.READABLE);
  bool readyWrite() => _ready(MojoHandleSignals.WRITABLE);

  static MojoResult waitMany(List<int> handles,
                             List<int> signals,
                             int deadline) {
    if (handles.length != signals.length) {
      return MojoResult.INVALID_ARGUMENT;
    }
    int result = _MojoHandleNatives.waitMany(
        handles, signals, handles.length, deadline);
    return new MojoResult(result);
  }

  static bool isValid(RawMojoHandle h) => (h.h != INVALID);

  int h;
}
