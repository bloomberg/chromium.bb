// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of bindings;

abstract class Client {
  core.MojoMessagePipeEndpoint _endpoint;
  core.MojoHandle _handle;
  List _sendQueue;
  List _completerQueue;
  bool _isOpen = false;

  void handleResponse(MessageReader reader);

  Client(this._endpoint) {
    _sendQueue = [];
    _completerQueue = [];
    _handle = new core.MojoHandle(_endpoint.handle);
  }

  void open() {
    _handle.listen((int mojoSignal) {
      if (core.MojoHandleSignals.isReadable(mojoSignal)) {
        // Query how many bytes are available.
        var result = _endpoint.query();
        if (!result.status.isOk && !result.status.isResourceExhausted) {
          // If something else happens, it means the handle wasn't really ready
          // for reading, which indicates a bug in MojoHandle or the
          // handle watcher.
          throw new Exception("message pipe query failed: ${result.status}");
        }

        // Read the data.
        var bytes = new ByteData(result.bytesRead);
        var handles = new List<core.RawMojoHandle>(result.handlesRead);
        result = _endpoint.read(bytes, result.bytesRead, handles);
        if (!result.status.isOk && !result.status.isResourceExhausted) {
          throw new Exception("message pipe read failed: ${result.status}");
        }
        var message = new Message(bytes, handles);
        var reader = new MessageReader(message);

        handleResponse(reader);
      }
      if (core.MojoHandleSignals.isWritable(mojoSignal)) {
        if (_sendQueue.length > 0) {
          List messageCompleter = _sendQueue.removeAt(0);
          _endpoint.write(messageCompleter[0].buffer);
          if (!_endpoint.status.isOk) {
            throw new Exception("message pipe write failed");
          }
          if (messageCompleter[1] != null) {
            _completerQueue.add(messageCompleter[1]);
          }
        }
        if ((_sendQueue.length == 0) && _handle.writeEnabled()) {
          _handle.disableWriteEvents();
        }
      }
      if (core.MojoHandleSignals.isNone(mojoSignal)) {
        // The handle watcher will send MojoHandleSignals.NONE if the other
        // endpoint of the pipe is closed.
        _handle.close();
      }
    });
    _isOpen = true;
  }

  void close() {
    assert(isOpen);
    _handle.close();
    _isOpen = false;
  }

  void enqueueMessage(Type t, int name, Object msg) {
    var builder = new MessageBuilder(name, align(getEncodedSize(t)));
    builder.encodeStruct(t, msg);
    var message = builder.finish();
    _sendQueue.add([message, null]);
    if ((_sendQueue.length > 0) && !_handle.writeEnabled()) {
      _handle.enableWriteEvents();
    }
  }

  Future enqueueMessageWithRequestID(Type t, int name, int id, Object msg) {
    var builder = new MessageWithRequestIDBuilder(
        name, align(getEncodedSize(t)), id);
    builder.encodeStruct(t, msg);
    var message = builder.finish();

    var completer = new Completer();
    _sendQueue.add([message, completer]);
    if ((_sendQueue.length > 0) && !_handle.writeEnabled()) {
      _handle.enableWriteEvents();
    }
    return completer.future;
  }

  // Need a getter for this for access in subclasses.
  List get completerQueue => _completerQueue;
  bool get isOpen => _isOpen;
}
