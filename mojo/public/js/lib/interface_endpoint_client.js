// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/lib/interface_endpoint_client", [
  "console",
  "mojo/public/js/codec",
  "mojo/public/js/lib/control_message_handler",
  "mojo/public/js/lib/control_message_proxy",
  "mojo/public/js/lib/interface_endpoint_handle",
  "mojo/public/js/validator",
  "timer",
], function(console,
            codec,
            controlMessageHandler,
            controlMessageProxy,
            interfaceEndpointHandle,
            validator,
            timer) {

  var ControlMessageHandler = controlMessageHandler.ControlMessageHandler;
  var ControlMessageProxy = controlMessageProxy.ControlMessageProxy;
  var MessageReader = codec.MessageReader;
  var Validator = validator.Validator;
  var InterfaceEndpointHandle = interfaceEndpointHandle.InterfaceEndpointHandle;

  function InterfaceEndpointClient(interfaceEndpointHandle, receiver,
      interfaceVersion) {
    this.controller_ = null;
    this.encounteredError_ = false;
    this.handle_ = interfaceEndpointHandle;
    this.incomingReceiver_ = receiver;

    if (interfaceVersion !== undefined) {
      this.controlMessageHandler_ = new ControlMessageHandler(
          interfaceVersion);
    } else {
      this.controlMessageProxy_ = new ControlMessageProxy(this);
    }

    this.nextRequestID_ = 0;
    this.completers_ = new Map();
    this.payloadValidators_ = [];
    this.connectionErrorHandler_ = null;

    if (interfaceEndpointHandle.pendingAssociation()) {
      interfaceEndpointHandle.setAssociationEventHandler(
          this.onAssociationEvent.bind(this));
    } else {
      this.initControllerIfNecessary_();
    }
  }

  InterfaceEndpointClient.prototype.initControllerIfNecessary_ = function() {
    if (this.controller_ || this.handle_.pendingAssociation()) {
      return;
    }

    this.controller_ = this.handle_.groupController().attachEndpointClient(
        this.handle_, this);
  };

  InterfaceEndpointClient.prototype.onAssociationEvent = function(
      associationEvent) {
    if (associationEvent ===
        InterfaceEndpointHandle.AssociationEvent.ASSOCIATED) {
      this.initControllerIfNecessary_();
    } else if (associationEvent ===
        InterfaceEndpointHandle.AssociationEvent
                               .PEER_CLOSED_BEFORE_ASSOCIATION) {
      timer.createOneShot(0, this.notifyError.bind(this,
          this.handle_.disconnectReason()));
    }
  };

  InterfaceEndpointClient.prototype.passHandle = function() {
    if (!this.handle_.isValid()) {
      return new InterfaceEndpointHandle();
    }

    // Used to clear the previously set callback.
    this.handle_.setAssociationEventHandler(undefined);

    if (this.controller_) {
      this.controller_ = null;
      this.handle_.groupController().detachEndpointClient(this.handle_);
    }
    var handle = this.handle_;
    this.handle_ = null;
    return handle;
  };

  InterfaceEndpointClient.prototype.close = function(reason) {
    var handle = this.passHandle();
    handle.reset(reason);
  };

  InterfaceEndpointClient.prototype.accept = function(message) {
    if (this.encounteredError_) {
      return false;
    }

    this.initControllerIfNecessary_();
    return this.controller_.sendMessage(message);
  };

  InterfaceEndpointClient.prototype.acceptAndExpectResponse = function(
      message) {
    if (this.encounteredError_) {
      return Promise.reject();
    }

    this.initControllerIfNecessary_();

    // Reserve 0 in case we want it to convey special meaning in the future.
    var requestID = this.nextRequestID_++;
    if (requestID === 0)
      requestID = this.nextRequestID_++;

    message.setRequestID(requestID);
    var result = this.controller_.sendMessage(message);
    if (!result)
      return Promise.reject(Error("Connection error"));

    var completer = {};
    this.completers_.set(requestID, completer);
    return new Promise(function(resolve, reject) {
      completer.resolve = resolve;
      completer.reject = reject;
    });
  };

  InterfaceEndpointClient.prototype.setPayloadValidators = function(
      payloadValidators) {
    this.payloadValidators_ = payloadValidators;
  };

  InterfaceEndpointClient.prototype.setIncomingReceiver = function(receiver) {
    this.incomingReceiver_ = receiver;
  };

  InterfaceEndpointClient.prototype.setConnectionErrorHandler = function(
      handler) {
    this.connectionErrorHandler_ = handler;
  };

  InterfaceEndpointClient.prototype.handleIncomingMessage_ = function(
      message) {
    var noError = validator.validationError.NONE;
    var messageValidator = new Validator(message);
    var err = noError;
    for (var i = 0; err === noError && i < this.payloadValidators_.length; ++i)
      err = this.payloadValidators_[i](messageValidator);

    if (err == noError) {
      return this.handleValidIncomingMessage_(message);
    } else {
      validator.reportValidationError(err);
      return false;
    }
  };

  InterfaceEndpointClient.prototype.handleValidIncomingMessage_ = function(
      message) {
    if (validator.isTestingMode()) {
      return true;
    }

    if (this.encounteredError_) {
      return false;
    }

    var ok = false;

    if (message.expectsResponse()) {
      if (controlMessageHandler.isControlMessage(message) &&
          this.controlMessageHandler_) {
        ok = this.controlMessageHandler_.acceptWithResponder(message, this);
      } else if (this.incomingReceiver_) {
        ok = this.incomingReceiver_.acceptWithResponder(message, this);
      }
    } else if (message.isResponse()) {
      var reader = new MessageReader(message);
      var requestID = reader.requestID;
      var completer = this.completers_.get(requestID);
      if (completer) {
        this.completers_.delete(requestID);
        completer.resolve(message);
        ok = true;
      } else {
        console.log("Unexpected response with request ID: " + requestID);
      }
    } else {
      if (controlMessageHandler.isControlMessage(message) &&
          this.controlMessageHandler_) {
        ok = this.controlMessageHandler_.accept(message);
      } else if (this.incomingReceiver_) {
        ok = this.incomingReceiver_.accept(message);
      }
    }
    return ok;
  };

  InterfaceEndpointClient.prototype.notifyError = function(reason) {
    if (this.encounteredError_) {
      return;
    }
    this.encounteredError_ = true;

    this.completers_.forEach(function(value) {
      value.reject();
    });
    this.completers_.clear();  // Drop any responders.

    if (this.connectionErrorHandler_) {
      this.connectionErrorHandler_(reason);
    }
  };

  InterfaceEndpointClient.prototype.queryVersion = function() {
    return this.controlMessageProxy_.queryVersion();
  };

  InterfaceEndpointClient.prototype.requireVersion = function(version) {
    this.controlMessageProxy_.requireVersion(version);
  };

  var exports = {};
  exports.InterfaceEndpointClient = InterfaceEndpointClient;

  return exports;
});
