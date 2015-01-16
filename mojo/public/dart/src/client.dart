// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of bindings;

abstract class Client {
  core.MojoMessagePipeEndpoint _endpoint;
  core.MojoEventStream _eventStream;
  List _sendQueue;
  Map<int, Completer> _completerMap;
  bool _isOpen = false;
  int _nextId = 0;

  void handleResponse(ServiceMessage reader);

  Client(core.MojoMessagePipeEndpoint endpoint) :
      _sendQueue = [],
      _completerMap = {},
      _endpoint = endpoint,
      _eventStream = new core.MojoEventStream(endpoint.handle);

  Client.fromHandle(int handle) {
    _sendQueue = [];
    _completerMap = {};
    _endpoint =
        new core.MojoMessagePipeEndpoint(new core.MojoHandle(handle));
    _eventStream = new core.MojoHandle(_endpoint.handle);
  }

  void _doRead() {
    // Query how many bytes are available.
    var result = _endpoint.query();
    assert(result.status.isOk || result.status.isResourceExhausted);

    // Read the data.
    var bytes = new ByteData(result.bytesRead);
    var handles = new List<core.MojoHandle>(result.handlesRead);
    result = _endpoint.read(bytes, result.bytesRead, handles);
    assert(result.status.isOk || result.status.isResourceExhausted);
    var message = new ServiceMessage.fromMessage(new Message(bytes, handles));
    handleResponse(message);
  }

  void _doWrite() {
    if (_sendQueue.length > 0) {
      List messageCompleter = _sendQueue.removeAt(0);
      ServiceMessage message = messageCompleter[0];
      Completer completer = messageCompleter[1];
      _endpoint.write(message.buffer,
                      message.buffer.lengthInBytes,
                      message.handles);
      if (!_endpoint.status.isOk) {
        throw "message pipe write failed";
      }
      if (completer != null) {
        if (!message.header.hasRequestId) {
          throw "Message has a completer, but does not expect a response";
        }
        int requestId = message.header.requestId;
        if (_completerMap[requestId] != null) {
          throw "Request Id $requestId is already in use.";
        }
        _completerMap[requestId] = completer;
      }
    }
  }

  void open() {
    _eventStream.listen((List<int> event) {
      var signalsWatched = new core.MojoHandleSignals(event[0]);
      var signalsReceived = new core.MojoHandleSignals(event[1]);
      if (signalsReceived.isPeerClosed) {
        close();
        return;
      }

      if (signalsReceived.isReadable) {
        _doRead();
      }

      if (signalsReceived.isWritable) {
        _doWrite();
      }

      if (_sendQueue.length == 0) {
        var withoutWritable = signalsWatched - core.MojoHandleSignals.WRITABLE;
        _eventStream.enableSignals(withoutWritable);
      } else {
        _eventStream.enableSignals(signalsWatched);
      }
    });
    _isOpen = true;
  }

  void close() {
    if (_isOpen) {
      _eventStream.close();
      _eventStream = null;
      _isOpen = false;
    }
  }

  void enqueueMessage(Struct message, int name) {
    var header = new MessageHeader(name);
    var serviceMessage = message.serializeWithHeader(header);
    _sendQueue.add([serviceMessage, null]);
    if (_sendQueue.length == 1) {
      _eventStream.enableAllEvents();
    }
  }

  int _getNextId() {
    return _nextId++;
  }

  Future enqueueMessageWithRequestId(
      Struct message, int name, int id, int flags) {
    if (id == -1) {
      id = _getNextId();
    }

    var header = new MessageHeader.withRequestId(name, flags, id);
    var serviceMessage = message.serializeWithHeader(header);
    var completer = new Completer();
    _sendQueue.add([serviceMessage, completer]);
    if (_sendQueue.length == 1) {
      _eventStream.enableAllEvents();
    } else {
      _eventStream.enableReadEvents();
    }
    return completer.future;
  }

  // Need a getter for this for access in subclasses.
  Map<int, Completer> get completerMap => _completerMap;
  bool get isOpen => _isOpen;
}
