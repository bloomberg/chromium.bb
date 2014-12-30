// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of bindings;

abstract class Interface {
  core.MojoMessagePipeEndpoint _endpoint;
  core.MojoHandle _handle;
  List _sendQueue;
  bool _isOpen;

  Message handleMessage(MessageReader reader);

  Interface(this._endpoint) {
    _sendQueue = [];
    _handle = new core.MojoHandle(_endpoint.handle);
    _isOpen = false;
  }

  StreamSubscription<int> listen() {
    _isOpen = true;
    return _handle.listen((int mojoSignal) {
      if (core.MojoHandleSignals.isReadable(mojoSignal)) {
        // Query how many bytes are available.
        var result = _endpoint.query();
        if (!result.status.isOk && !result.status.isResourceExhausted) {
          // If something else happens, it means the handle wasn't really ready
          // for reading, which indicates a bug in MojoHandle or the
          // event listener.
          throw new Exception("message pipe query failed: ${result.status}");
        }

        // Read the data and view as a message.
        var bytes = new ByteData(result.bytesRead);
        var handles = new List<core.RawMojoHandle>(result.handlesRead);
        result = _endpoint.read(bytes, result.bytesRead, handles);
        if (!result.status.isOk && !result.status.isResourceExhausted) {
          // If something else happens, it means the handle wasn't really ready
          // for reading, which indicates a bug in MojoHandle or the
          // event listener.
          throw new Exception("message pipe read failed: ${result.status}");
        }
        var message = new Message(bytes, handles);
        var reader = new MessageReader(message);

        // Prepare the response.
        var responseMessage = handleMessage(reader);
        // If there's a response, queue it up for sending.
        if (responseMessage != null) {
          _sendQueue.add(responseMessage);
          if ((_sendQueue.length > 0) && !_handle.writeEnabled()) {
            _handle.enableWriteEvents();
          }
        }
      }
      if (core.MojoHandleSignals.isWritable(mojoSignal)) {
        if (_sendQueue.length > 0) {
          var responseMessage = _sendQueue.removeAt(0);
          _endpoint.write(responseMessage.buffer,
                          responseMessage.buffer.lengthInBytes,
                          responseMessage.handles);
          if (!_endpoint.status.isOk) {
            throw new Exception("message pipe write failed");
          }
        }
        if ((_sendQueue.length == 0) && _handle.writeEnabled()) {
          _handle.disableWriteEvents();
        }
      }
      if (core.MojoHandleSignals.isNone(mojoSignal)) {
        // The handle watcher will send MojoHandleSignals.NONE when the other
        // endpoint of the pipe is closed.
        _handle.close();
      }
    });
  }

  Message buildResponse(Type t, int name, Object response) {
    var builder = new MessageBuilder(name, align(getEncodedSize(t)));
    builder.encodeStruct(t, response);
    return builder.finish();
  }

  Message buildResponseWithID(
      Type t, int name, int id, int flags, Object response) {
    var builder = new MessageWithRequestIDBuilder(
        name, align(getEncodedSize(t)), id, flags);
    builder.encodeStruct(t, response);
    return builder.finish();
  }

  void enqueueMessage(Type t, int name, Object msg) {
    var builder = new MessageBuilder(name, align(getEncodedSize(t)));
    builder.encodeStruct(t, msg);
    var message = builder.finish();
    _sendQueue.add(message);
    if (!_handle.writeEnabled()) {
      _handle.enableWriteEvents();
    }
  }

  Future enqueueMessageWithRequestID(Type t, int name, int id, Object msg) {
    throw new Exception("The client Mixin should not expect a response");
  }

  bool get isOpen => _isOpen;
}
