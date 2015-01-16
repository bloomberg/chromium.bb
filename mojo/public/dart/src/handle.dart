// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of core;

class _MojoHandleNatives {
  static int register(MojoEventStream eventStream) native "MojoHandle_Register";
  static int close(int handle) native "MojoHandle_Close";
  static List wait(int handle, int signals, int deadline)
      native "MojoHandle_Wait";
  static List waitMany(
      List<int> handles, List<int> signals, int deadline)
      native "MojoHandle_WaitMany";
}


class MojoHandle {
  static const int INVALID = 0;
  static const int DEADLINE_INDEFINITE = -1;

  int h;

  MojoHandle(this.h);

  MojoResult close() {
    int result = _MojoHandleNatives.close(h);
    h = INVALID;
    return new MojoResult(result);
  }

  MojoWaitResult wait(int signals, int deadline) {
    List result = _MojoHandleNatives.wait(h, signals, deadline);
    return new MojoWaitResult(new MojoResult(result[0]), result[1]);
  }

  bool _ready(MojoHandleSignals signal) {
    MojoWaitResult mwr = wait(signal.value, 0);
    switch (mwr.result) {
      case MojoResult.OK:
        return true;
      case MojoResult.DEADLINE_EXCEEDED:
      case MojoResult.CANCELLED:
      case MojoResult.INVALID_ARGUMENT:
      case MojoResult.FAILED_PRECONDITION:
        return false;
      default:
        // Should be unreachable.
        throw "Unexpected result $res for wait on $h";
    }
  }

  bool get readyRead => _ready(MojoHandleSignals.READABLE);
  bool get readyWrite => _ready(MojoHandleSignals.WRITABLE);

  static MojoWaitManyResult waitMany(
      List<int> handles, List<int> signals, int deadline) {
    List result = _MojoHandleNatives.waitMany(handles, signals, deadline);
    return new MojoWaitManyResult(
        new MojoResult(result[0]), result[1], result[2]);
  }

  static MojoResult register(MojoEventStream eventStream) {
    return new MojoResult(_MojoHandleNatives.register(eventStream));
  }

  bool get isValid => (h != INVALID);

  String toString() => "$h";

  bool operator ==(MojoHandle other) {
    return h == other.h;
  }
}


class MojoEventStream extends Stream<int> {
  // The underlying Mojo handle.
  MojoHandle _handle;

  // Providing our own stream controller allows us to take custom actions when
  // listeners pause/resume/etc. their StreamSubscription.
  StreamController _controller;

  // The send port that we give to the handle watcher to notify us of handle
  // events.
  SendPort _sendPort;

  // The receive port on which we listen and receive events from the handle 
  // watcher.
  ReceivePort _receivePort;

  // The signals on this handle that we're interested in.
  MojoHandleSignals _signals;

  // Whether listen has been called.
  bool _isListening;

  MojoEventStream(MojoHandle handle,
                  [MojoHandleSignals signals = MojoHandleSignals.READABLE]) :
      _handle = handle,
      _signals = signals,
      _isListening = false {
    MojoResult result = MojoHandle.register(this);
    if (!result.isOk) {
      throw "Failed to register the MojoHandle: $result.";
    }
  }

  void close() {
    if (_handle != null) {
      MojoHandleWatcher.close(_handle);
      _handle = null;
    }
    if (_receivePort != null) {
      _receivePort.close();
      _receivePort = null;
    }
  }

  StreamSubscription<List<int>> listen(
      void onData(List event),
      {Function onError, void onDone(), bool cancelOnError}) {
    if (_isListening) {
      throw "Listen has already been called: $_handle.";
    }
    _receivePort = new ReceivePort();
    _sendPort = _receivePort.sendPort;
    _controller = new StreamController(sync: true,
        onListen: _onSubscriptionStateChange,
        onCancel: _onSubscriptionStateChange,
        onPause: _onPauseStateChange,
        onResume: _onPauseStateChange);
    _controller.addStream(_receivePort);

    if (_signals != MojoHandleSignals.NONE) {
      var res = MojoHandleWatcher.add(_handle, _sendPort, _signals.value);
      if (!res.isOk) {
        throw "MojoHandleWatcher add failed: $res";
      }
    }

    _isListening = true;
    return _controller.stream.listen(
        onData,
        onError: onError,
        onDone: onDone,
        cancelOnError: cancelOnError);
  }

  void enableSignals(MojoHandleSignals signals) {
    _signals = signals;
    if (_isListening) {
      var res = MojoHandleWatcher.add(_handle, _sendPort, signals.value);
      if (!res.isOk) {
        throw "MojoHandleWatcher add failed: $res";
      }
    }
  }

  void enableReadEvents() => enableSignals(MojoHandleSignals.READABLE);
  void enableWriteEvents() => enableSignals(MojoHandleSignals.WRITABLE);
  void enableAllEvents() => enableSignals(MojoHandleSignals.READWRITE);

  void _onSubscriptionStateChange() {
    if (!_controller.hasListener) {
      close();
    }
  }

  void _onPauseStateChange() {
    if (_controller.isPaused) {
      var res = MojoHandleWatcher.remove(_handle);
      if (!res.isOk) {
        throw "MojoHandleWatcher add failed: $res";
      }
    } else {
      var res = MojoHandleWatcher.add(_handle, _sendPort, _signals.value);
      if (!res.isOk) {
        throw "MojoHandleWatcher add failed: $res";
      }
    }
  }

  bool get readyRead => _handle.readyRead;
  bool get readyWrite => _handle.readyWrite;

  String toString() => "$_handle";
}
