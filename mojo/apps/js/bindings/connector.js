// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
  "mojo/apps/js/bindings/codec",
  "mojo/apps/js/bindings/core",
  "mojo/apps/js/bindings/support",
], function(codec, core, support) {

  function Connector(handle) {
    this.handle_ = handle;
    this.error_ = false;
    this.incomingReceiver_ = null;
    this.readWaitCookie_ = null;
  }

  Connector.prototype.close = function() {
    if (this.readWaitCookie_) {
      support.cancelWait(this.readWaitCookie_);
      this.readWaitCookie_ = null;
    }
    if (this.handle_ != core.kInvalidHandle) {
      core.close(this.handle_);
      this.handle_ = core.kInvalidHandle;
    }
  };

  Connector.prototype.accept = function(message) {
    if (this.error_)
      return false;
    this.write_(message);
    return !this.error_;
  };

  Connector.prototype.setIncomingReceiver = function(receiver) {
    this.incomingReceiver_ = receiver;
    if (this.incomingReceiver_)
      this.waitToReadMore_();
  };

  Connector.prototype.write_ = function(message) {
    var result = core.writeMessage(this.handle_,
                                   message.memory,
                                   message.handles,
                                   core.WRITE_MESSAGE_FLAG_NONE);
    if (result != core.RESULT_OK) {
      this.error_ = true
      return;
    }
    // The handles were successfully transferred, so we don't own them anymore.
    message.handles = [];
  };

  Connector.prototype.waitToReadMore_ = function() {
    this.readWaitCookie_ = support.asyncWait(this.handle_,
                                             core.WAIT_FLAG_READABLE,
                                             this.readMore_.bind(this));
  };

  Connector.prototype.readMore_ = function(result) {
    for (;;) {
      var read = core.readMessage(this.handle_,
                                  core.READ_MESSAGE_FLAG_NONE);
      if (read.result == core.RESULT_NOT_FOUND) {
        this.waitToReadMore_();
        return;
      }
      if (read.result != core.RESULT_OK) {
        this.error_ = true;
        return;
      }
      // TODO(abarth): Should core.readMessage return a Uint8Array?
      var memory = new Uint8Array(read.buffer);
      var message = new codec.Message(memory, read.handles);
      this.incomingReceiver_.accept(message);
    }
  };

  function Connection(handle, localFactory, remoteFactory) {
    this.connector_ = new Connector(handle);
    this.remote = new remoteFactory(this.connector_);
    this.local = new localFactory(this.remote);
    this.connector_.setIncomingReceiver(this.local);
  }

  Connection.prototype.close = function() {
    this.connector_.close();
    this.connector_ = null;
    this.local = null;
    this.remote = null;
  };

  var exports = {};
  exports.Connection = Connection;
  return exports;
});
