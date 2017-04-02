// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  function Connector(handle) {
    if (!(handle instanceof MojoHandle))
      throw new Error("Connector: not a handle " + handle);
    this.handle_ = handle;
    this.dropWrites_ = false;
    this.error_ = false;
    this.incomingReceiver_ = null;
    this.readWatcher_ = null;
    this.errorHandler_ = null;

    if (handle) {
      this.readWatcher_ = handle.watch({readable: true},
                                       this.readMore_.bind(this));
    }
  }

  Connector.prototype.close = function() {
    if (this.readWatcher_) {
      this.readWatcher_.cancel();
      this.readWatcher_ = null;
    }
    if (this.handle_ != null) {
      this.handle_.close();
      this.handle_ = null;
    }
  };

  Connector.prototype.accept = function(message) {
    if (this.error_)
      return false;

    if (this.dropWrites_)
      return true;

    var result = this.handle_.writeMessage(
        new Uint8Array(message.buffer.arrayBuffer), message.handles);
    switch (result) {
      case Mojo.RESULT_OK:
        // The handles were successfully transferred, so we don't own them
        // anymore.
        message.handles = [];
        break;
      case Mojo.RESULT_FAILED_PRECONDITION:
        // There's no point in continuing to write to this pipe since the other
        // end is gone. Avoid writing any future messages. Hide write failures
        // from the caller since we'd like them to continue consuming any
        // backlog of incoming messages before regarding the message pipe as
        // closed.
        this.dropWrites_ = true;
        break;
      default:
        // This particular write was rejected, presumably because of bad input.
        // The pipe is not necessarily in a bad state.
        return false;
    }
    return true;
  };

  Connector.prototype.setIncomingReceiver = function(receiver) {
    this.incomingReceiver_ = receiver;
  };

  Connector.prototype.setErrorHandler = function(handler) {
    this.errorHandler_ = handler;
  };

  Connector.prototype.encounteredError = function() {
    return this.error_;
  };

  Connector.prototype.waitForNextMessageForTesting = function() {
    // TODO(yzshen): Change the tests that use this method.
    throw new Error("Not supported!");
  };

  Connector.prototype.readMore_ = function(result) {
    for (;;) {
      var read = this.handle_.readMessage();
      if (this.handle_ == null) // The connector has been closed.
        return;
      if (read.result == Mojo.RESULT_SHOULD_WAIT)
        return;
      if (read.result != Mojo.RESULT_OK) {
        this.error_ = true;
        if (this.errorHandler_)
          this.errorHandler_.onError(read.result);
        return;
      }
      var messageBuffer = new internal.Buffer(read.buffer);
      var message = new internal.Message(messageBuffer, read.handles);
      if (this.incomingReceiver_)
        this.incomingReceiver_.accept(message);
    }
  };

  internal.Connector = Connector;
})();
