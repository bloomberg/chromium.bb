// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of core;

class _MojoHandleWatcherNatives {
  static int sendControlData(
      int controlHandle, int mojoHandle, SendPort port, int data)
      native "MojoHandleWatcher_SendControlData";
  static List recvControlData(int controlHandle)
      native "MojoHandleWatcher_RecvControlData";
  static int setControlHandle(int controlHandle)
      native "MojoHandleWatcher_SetControlHandle";
  static int getControlHandle()
      native "MojoHandleWatcher_GetControlHandle";
}

// The MojoHandleWatcher sends a stream of events to application isolates that
// register Mojo handles with it. Application isolates make the following calls:
//
// Start() - Starts up the MojoHandleWatcher isolate. Should be called only once
//           per VM process.
//
// Stop() - Causes the MojoHandleWatcher isolate to exit.
//
// add(handle, port, signals) - Instructs the MojoHandleWatcher isolate to add
//     'handle' to the set of handles it watches, and to notify the calling
//     isolate only for the events specified by 'signals' using the send port
//     'port'
//
// remove(handle) - Instructs the MojoHandleWatcher isolate to remove 'handle'
//     from the set of handles it watches. This allows the application isolate
//     to, e.g., pause the stream of events.
//
// toggleWrite(handle) - Modifies the set of signals that an application isolate
//     wishes to be notified about. Whether the application isolate should be
//     be notified about a handle ready for writing is made the opposite.
//
// close(handle) - Notifies the HandleWatcherIsolate that a handle it is
//     watching should be removed from its set and closed.
class MojoHandleWatcher {
  // Control commands.
  static const int ADD = 0;
  static const int REMOVE = 1;
  static const int TOGGLE_WRITE = 2;
  static const int CLOSE = 3;
  static const int SHUTDOWN = 4;

  static int _encodeCommand(int cmd, [int signals = 0]) =>
      (cmd << 2) | (signals & MojoHandleSignals.READWRITE);
  static int _decodeCommand(int cmd) => cmd >> 2;

  // The Mojo handle over which control messages are sent.
  int _controlHandle;

  // Whether the handle watcher should shut down.
  bool _shutdown;

  // The list of handles being watched.
  List<int> _handles;
  int _handleCount;

  // A port for each handle on which to send events back to the isolate that
  // owns the handle.
  List<SendPort> _ports;

  // The signals that we care about for each handle.
  List<int> _signals;

  // A mapping from Mojo handles to their indices in _handles.
  Map<int, int> _handleIndices;

  // Since we are not storing wrapped handles, a dummy handle for when we need
  // a RawMojoHandle.
  RawMojoHandle _tempHandle;

  MojoHandleWatcher(this._controlHandle) :
      _shutdown = false,
      _handles = new List<int>(),
      _ports = new List<SendPort>(),
      _signals = new List<int>(),
      _handleIndices = new Map<int, int>(),
      _handleCount = 1,
      _tempHandle = new RawMojoHandle(RawMojoHandle.INVALID) {
    // Setup control handle.
    _handles.add(_controlHandle);
    _ports.add(null);  // There is no port for the control handle.
    _signals.add(MojoHandleSignals.READABLE);
    _handleIndices[_controlHandle] = 0;
  }

  static void _handleWatcherIsolate(MojoHandleWatcher watcher) {
    while (!watcher._shutdown) {
      int res = RawMojoHandle.waitMany(watcher._handles,
                                       watcher._signals,
                                       RawMojoHandle.DEADLINE_INDEFINITE);
      if (res == 0) {
        watcher._handleControlMessage();
      } else if (res > 0) {
        int handle = watcher._handles[res];
        // Route event.
        watcher._routeEvent(res);
        // Remove the handle from the list.
        watcher._removeHandle(handle);
      } else {
        // Some handle was closed, but not by us.
        // We have to go through the list and find it.
        watcher._pruneClosedHandles();
      }
    }
  }

  void _routeEvent(int idx) {
    int client_handle = _handles[idx];
    int signals = _signals[idx];
    SendPort port = _ports[idx];

    _tempHandle.h = client_handle;
    bool readyWrite =
        MojoHandleSignals.isWritable(signals) && _tempHandle.readyWrite();
    bool readyRead =
        MojoHandleSignals.isReadable(signals) && _tempHandle.readyRead();
    if (readyRead && readyWrite) {
      port.send(MojoHandleSignals.READWRITE);
    } else if (readyWrite) {
      port.send(MojoHandleSignals.WRITABLE);
    } else if (readyRead) {
      port.send(MojoHandleSignals.READABLE);
    }
    _tempHandle.h = RawMojoHandle.INVALID;
  }

  void _handleControlMessage() {
    List result = _MojoHandleWatcherNatives.recvControlData(_controlHandle);
    // result[0] = mojo handle if any
    // result[1] = SendPort if any
    // result[2] = command << 2 | WRITABLE | READABLE

    int signals = result[2] & MojoHandleSignals.READWRITE;
    int command = _decodeCommand(result[2]);
    switch (command) {
      case ADD:
        _addHandle(result[0], result[1], signals);
        break;
      case REMOVE:
        _removeHandle(result[0]);
        break;
      case TOGGLE_WRITE:
        _toggleWrite(result[0]);
        break;
      case CLOSE:
        _close(result[0]);
        break;
      case SHUTDOWN:
        _shutdownHandleWatcher();
        break;
      default:
        throw new Exception("Invalid Command: $command");
        break;
    }
  }

  void _addHandle(int mojoHandle, SendPort port, int signals) {
    _handles.add(mojoHandle);
    _ports.add(port);
    _signals.add(signals & MojoHandleSignals.READWRITE);
    _handleIndices[mojoHandle] = _handleCount;
    _handleCount++;
  }

  void _removeHandle(int mojoHandle) {
    int idx = _handleIndices[mojoHandle];
    if (idx == null) {
      throw new Exception("Remove on a non-existent handle: idx = $idx.");
    }
    if (idx == 0) {
      throw new Exception("The control handle (idx = 0) cannot be removed.");
    }
    // We don't use List.removeAt so that we know how to fix-up _handleIndices.
    if (idx == _handleCount - 1) {
      int handle = _handles[idx];
      _handleIndices[handle] = null;
      _handles.removeLast();
      _signals.removeLast();
      _ports.removeLast();
      _handleCount--;
    } else {
      int last = _handleCount - 1;
      _handles[idx] = _handles[last];
      _signals[idx] = _signals[last];
      _ports[idx] = _ports[last];
      _handles.removeLast();
      _signals.removeLast();
      _ports.removeLast();
      _handleIndices[_handles[idx]] = idx;
      _handleCount--;
    }
  }

  void _close(int mojoHandle, {bool pruning : false}) {
    int idx = _handleIndices[mojoHandle];
    if (idx == null) {
      throw new Exception("Close on a non-existent handle: idx = $idx.");
    }
    if (idx == 0) {
      throw new Exception("The control handle (idx = 0) cannot be closed.");
    }
    _tempHandle.h = _handles[idx];
    _tempHandle.close();
    _tempHandle.h = RawMojoHandle.INVALID;
    if (pruning) {
      // If this handle is being pruned, notify the application isolate
      // by sending MojoHandleSignals.NONE.
      _ports[idx].send(MojoHandleSignals.NONE);
    }
    _removeHandle(mojoHandle);
  }

  void _toggleWrite(int mojoHandle) {
    int idx = _handleIndices[mojoHandle];
    if (idx == null) {
      throw new Exception(
          "Toggle write on a non-existent handle: $mojoHandle.");
    }
    if (idx == 0) {
      throw new Exception("The control handle (idx = 0) cannot be toggled.");
    }
    _signals[idx] = MojoHandleSignals.toggleWrite(_signals[idx]);
  }

  void _pruneClosedHandles() {
    List<int> closed = new List();
    for (var h in _handles) {
      _tempHandle.h = h;
      MojoResult res = _tempHandle.wait(MojoHandleSignals.READWRITE, 0);
      if ((!res.isOk) && (!res.isDeadlineExceeded)) {
        closed.add(h);
      }
      _tempHandle.h = RawMojoHandle.INVALID;
    }
    for (var h in closed) {
      _close(h, pruning: true);
    }
  }

  void _shutdownHandleWatcher() {
    _shutdown = true;
    _tempHandle.h = _controlHandle;
    _tempHandle.close();
    _tempHandle.h = RawMojoHandle.INVALID;
  }

  static MojoResult _sendControlData(RawMojoHandle mojoHandle,
                                     SendPort port,
                                     int data) {
    int controlHandle = _MojoHandleWatcherNatives.getControlHandle();
    if (controlHandle == RawMojoHandle.INVALID) {
      throw new Exception("Found invalid control handle");
    }

    int rawHandle = RawMojoHandle.INVALID;
    if (mojoHandle != null) {
      rawHandle = mojoHandle.h;
    }
    var result = _MojoHandleWatcherNatives.sendControlData(
        controlHandle, rawHandle, port, data);
    return new MojoResult(result);
  }

  static Future<Isolate> Start() {
    // Make a control message pipe,
    MojoMessagePipe pipe = new MojoMessagePipe();
    int consumerHandle = pipe.endpoints[0].handle.h;
    int producerHandle = pipe.endpoints[1].handle.h;

    // Make a MojoHandleWatcher with one end.
    MojoHandleWatcher watcher = new MojoHandleWatcher(consumerHandle);

    // Call setControlHandle with the other end.
    _MojoHandleWatcherNatives.setControlHandle(producerHandle);

    // Spawn the handle watcher isolate with the MojoHandleWatcher,
    return Isolate.spawn(_handleWatcherIsolate, watcher);
  }

  static void Stop() {
    // Send the shutdown command.
    _sendControlData(null, null, _encodeCommand(SHUTDOWN));

    // Close the control handle.
    int controlHandle = _MojoHandleWatcherNatives.getControlHandle();
    var handle = new RawMojoHandle(controlHandle);
    handle.close();
  }

  static MojoResult close(RawMojoHandle mojoHandle) {
    return _sendControlData(mojoHandle, null, _encodeCommand(CLOSE));
  }

  static MojoResult toggleWrite(RawMojoHandle mojoHandle) {
    return _sendControlData(mojoHandle, null, _encodeCommand(TOGGLE_WRITE));
  }

  static MojoResult add(RawMojoHandle mojoHandle, SendPort port, int signals) {
    return _sendControlData(mojoHandle, port, _encodeCommand(ADD, signals));
  }

  static MojoResult remove(RawMojoHandle mojoHandle) {
    return _sendControlData(mojoHandle, null, _encodeCommand(REMOVE));
  }
}
