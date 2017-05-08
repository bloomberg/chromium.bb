// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/connector", [
  "mojo/public/js/buffer",
  "mojo/public/js/codec",
  "mojo/public/js/core",
  "mojo/public/js/support",
  "mojo/public/js/validator",
], function(buffer, codec, core, support, validator) {

  function Connector(handle) {
    if (!core.isHandle(handle))
      throw new Error("Connector: not a handle " + handle);
    this.handle_ = handle;
    this.dropWrites_ = false;
    this.error_ = false;
    this.incomingReceiver_ = null;
    this.readWatcher_ = null;
    this.errorHandler_ = null;
    this.paused_ = false;

    this.waitToReadMore();
  }

  Connector.prototype.close = function() {
    this.cancelWait();
    if (this.handle_ != null) {
      core.close(this.handle_);
      this.handle_ = null;
    }
  };

  Connector.prototype.pauseIncomingMethodCallProcessing = function() {
    if (this.paused_) {
      return;
    }
    this.paused_= true;
    this.cancelWait();
  };

  Connector.prototype.resumeIncomingMethodCallProcessing = function() {
    if (!this.paused_) {
      return;
    }
    this.paused_= false;
    this.waitToReadMore();
  };

  Connector.prototype.accept = function(message) {
    if (this.error_)
      return false;

    if (this.dropWrites_)
      return true;

    var result = core.writeMessage(this.handle_,
                                   new Uint8Array(message.buffer.arrayBuffer),
                                   message.handles,
                                   core.WRITE_MESSAGE_FLAG_NONE);
    switch (result) {
      case core.RESULT_OK:
        // The handles were successfully transferred, so we don't own them
        // anymore.
        message.handles = [];
        break;
      case core.RESULT_FAILED_PRECONDITION:
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

  Connector.prototype.waitForNextMessageForTesting = function() {
    var wait = core.wait(this.handle_, core.HANDLE_SIGNAL_READABLE);
    this.readMore_(wait.result);
  };

  Connector.prototype.readMore_ = function(result) {
    for (;;) {
      if (this.paused_) {
        return;
      }

      var read = core.readMessage(this.handle_,
                                  core.READ_MESSAGE_FLAG_NONE);
      if (this.handle_ == null) // The connector has been closed.
        return;
      if (read.result == core.RESULT_SHOULD_WAIT)
        return;
      if (read.result != core.RESULT_OK) {
        this.handleError(read.result !== core.RESULT_FAILED_PRECONDITION,
            false);
        return;
      }
      var messageBuffer = new buffer.Buffer(read.buffer);
      var message = new codec.Message(messageBuffer, read.handles);
      var receiverResult = this.incomingReceiver_ &&
          this.incomingReceiver_.accept(message);

      // Handle invalid incoming message.
      if (!validator.isTestingMode() && !receiverResult) {
        // TODO(yzshen): Consider notifying the embedder.
        this.handleError(true, false);
      }
    }
  };

  Connector.prototype.cancelWait = function() {
    if (this.readWatcher_) {
      support.cancelWatch(this.readWatcher_);
      this.readWatcher_ = null;
    }
  };

  Connector.prototype.waitToReadMore = function() {
    if (this.handle_) {
      this.readWatcher_ = support.watch(this.handle_,
                                        core.HANDLE_SIGNAL_READABLE,
                                        this.readMore_.bind(this));
    }
  };

  Connector.prototype.handleError = function(forcePipeReset,
                                             forceAsyncHandler) {
    if (this.error_ || this.handle_ === null) {
      return;
    }

    if (this.paused_) {
      // Enforce calling the error handler asynchronously if the user has
      // paused receiving messages. We need to wait until the user starts
      // receiving messages again.
      forceAsyncHandler = true;
    }

    if (!forcePipeReset && forceAsyncHandler) {
      forcePipeReset = true;
    }

    this.cancelWait();
    if (forcePipeReset) {
      core.close(this.handle_);
      var dummyPipe = core.createMessagePipe();
      this.handle_ = dummyPipe.handle0;
    }

    if (forceAsyncHandler) {
      if (!this.paused_) {
        this.waitToReadMore();
      }
    } else {
      this.error_ = true;
      if (this.errorHandler_) {
        this.errorHandler_.onError();
      }
    }
  };

  var exports = {};
  exports.Connector = Connector;
  return exports;
});
