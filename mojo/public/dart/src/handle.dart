// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of core;

class _MojoHandleNatives {
  static int close(int handle) native "MojoHandle_Close";
  static int wait(int handle, int signals, int deadline)
      native "MojoHandle_Wait";
  static int waitMany(
      List<int> handles, List<int> signals, int num_handles, int deadline)
      native "MojoHandle_WaitMany";
}


class RawMojoHandle {
  static const int INVALID = 0;
  static const int DEADLINE_INDEFINITE = -1;

  int h;

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

  static int waitMany(List<int> handles,
                      List<int> signals,
                      int deadline) {
    if (handles.length != signals.length) {
      return MojoResult.kInvalidArgument;
    }
    return _MojoHandleNatives.waitMany(
        handles, signals, handles.length, deadline);
  }

  static bool isValid(RawMojoHandle h) => (h.h != INVALID);

  String toString() => "$h";
}


class MojoHandle extends Stream<int> {
  // The underlying Mojo handle.
  RawMojoHandle _handle;

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
  int _signals;

  // Whether the handle has been added to the handle watcher.
  bool _eventHandlerAdded;

  MojoHandle(this._handle) : 
      _signals = MojoHandleSignals.READABLE,
      _eventHandlerAdded = false,
      _receivePort = new ReceivePort() {
    _sendPort = _receivePort.sendPort;
    _controller = new StreamController(sync: true,
        onListen: _onSubscriptionStateChange,
        onCancel: _onSubscriptionStateChange,
        onPause: _onPauseStateChange,
        onResume: _onPauseStateChange);
    _controller.addStream(_receivePort);
  }

  void close() {
    if (_eventHandlerAdded) {
      MojoHandleWatcher.close(_handle);
    } else {
      // If we're not in the handle watcher, then close the handle manually.
      _handle.close();
    }
    _receivePort.close();
  }

  // We wrap the callback provided by clients in listen() with some code to
  // handle adding and removing the handle to/from the handle watcher. Because
  // the handle watcher removes this handle whenever it receives an event,
  // we have to re-add it when the callback is finished.
  Function _onDataClosure(origOnData) {
    return ((int event) {
      // The handle watcher removes this handle from its set on an event.
      _eventHandlerAdded = false;
      origOnData(event);

      // The callback could have closed the handle. If so, don't add it back to
      // the MojoHandleWatcher.
      if (RawMojoHandle.isValid(_handle)) {
        assert(!_eventHandlerAdded);
        var res = MojoHandleWatcher.add(_handle, _sendPort, _signals);
        if (!res.isOk) {
          throw new Exception("Failed to re-add handle: $_handle");
        }
        _eventHandlerAdded = true;
      }
    });
  }

  StreamSubscription<int> listen(
      void onData(int event),
      {Function onError, void onDone(), bool cancelOnError}) {

    assert(!_eventHandlerAdded);
    var res = MojoHandleWatcher.add(_handle, _sendPort, _signals);
    if (!res.isOk) {
      throw new Exception("MojoHandleWatcher add failed: $res");
    }
    _eventHandlerAdded = true;

    return _controller.stream.listen(
        _onDataClosure(onData),
        onError: onError,
        onDone: onDone,
        cancelOnError: cancelOnError);
  }

  bool writeEnabled() => MojoHandleSignals.isWritable(_signals);

  void toggleWriteEvents() {
    _signals = MojoHandleSignals.toggleWrite(_signals);
    if (_eventHandlerAdded) {
      var res = MojoHandleWatcher.toggleWrite(_handle);
      if (!res.isOk) {
        throw new Exception("MojoHandleWatcher failed to toggle write: $res");
      }
    }
  }

  void enableWriteEvents() {
    assert(!writeEnabled());
    toggleWriteEvents();
  }

  void disableWriteEvents() {
    assert(writeEnabled());
    toggleWriteEvents();
  }

  void _onSubscriptionStateChange() {
    if (!_controller.hasListener) {
      close();
    }
  }

  void _onPauseStateChange() {
    if (_controller.isPaused) {
      if (_eventHandlerAdded) {
        MojoHandleWatcher.remove(_handle);
        _eventHandlerAdded = false;
      }
    } else {
      if (!_eventHandlerAdded) {
        var res = MojoHandleWatcher.add(_handle, _sendPort, _signals);
        if (!res.isOk) {
          throw new Exception("MojoHandleWatcher add failed: $res");
        }
        _eventHandlerAdded = true;
      }
    }
  }

  String toString() => "$_handle";
}
