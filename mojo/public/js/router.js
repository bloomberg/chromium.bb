// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/router", [
  "console",
  "mojo/public/js/codec",
  "mojo/public/js/core",
  "mojo/public/js/connector",
  "mojo/public/js/lib/control_message_handler",
  "mojo/public/js/validator",
], function(console, codec, core, connector, controlMessageHandler, validator) {

  var Connector = connector.Connector;
  var MessageReader = codec.MessageReader;
  var Validator = validator.Validator;
  var ControlMessageHandler = controlMessageHandler.ControlMessageHandler;

  function Router(handle, interface_version, connectorFactory) {
    if (!core.isHandle(handle))
      throw new Error("Router constructor: Not a handle");
    if (connectorFactory === undefined)
      connectorFactory = Connector;
    this.connector_ = new connectorFactory(handle);
    this.incomingReceiver_ = null;
    this.errorHandler_ = null;
    this.nextRequestID_ = 0;
    this.completers_ = new Map();
    this.payloadValidators_ = [];
    this.testingController_ = null;

    if (interface_version !== undefined) {
      this.controlMessageHandler_ = new
          ControlMessageHandler(interface_version);
    }

    this.connector_.setIncomingReceiver({
        accept: this.handleIncomingMessage_.bind(this),
    });
    this.connector_.setErrorHandler({
        onError: this.handleConnectionError_.bind(this),
    });
  }

  Router.prototype.close = function() {
    this.completers_.clear();  // Drop any responders.
    this.connector_.close();
    this.testingController_ = null;
  };

  Router.prototype.accept = function(message) {
    this.connector_.accept(message);
  };

  Router.prototype.reject = function(message) {
    // TODO(mpcomplete): no way to trasmit errors over a Connection.
  };

  Router.prototype.acceptAndExpectResponse = function(message) {
    // Reserve 0 in case we want it to convey special meaning in the future.
    var requestID = this.nextRequestID_++;
    if (requestID == 0)
      requestID = this.nextRequestID_++;

    message.setRequestID(requestID);
    var result = this.connector_.accept(message);
    if (!result)
      return Promise.reject(Error("Connection error"));

    var completer = {};
    this.completers_.set(requestID, completer);
    return new Promise(function(resolve, reject) {
      completer.resolve = resolve;
      completer.reject = reject;
    });
  };

  Router.prototype.setIncomingReceiver = function(receiver) {
    this.incomingReceiver_ = receiver;
  };

  Router.prototype.setPayloadValidators = function(payloadValidators) {
    this.payloadValidators_ = payloadValidators;
  };

  Router.prototype.setErrorHandler = function(handler) {
    this.errorHandler_ = handler;
  };

  Router.prototype.encounteredError = function() {
    return this.connector_.encounteredError();
  };

  Router.prototype.enableTestingMode = function() {
    this.testingController_ = new RouterTestingController(this.connector_);
    return this.testingController_;
  };

  Router.prototype.handleIncomingMessage_ = function(message) {
    var noError = validator.validationError.NONE;
    var messageValidator = new Validator(message);
    var err = messageValidator.validateMessageHeader();
    for (var i = 0; err === noError && i < this.payloadValidators_.length; ++i)
      err = this.payloadValidators_[i](messageValidator);

    if (err == noError)
      this.handleValidIncomingMessage_(message);
    else
      this.handleInvalidIncomingMessage_(message, err);
  };

  Router.prototype.handleValidIncomingMessage_ = function(message) {
    if (this.testingController_)
      return;

    if (message.expectsResponse()) {
      if (controlMessageHandler.isControlMessage(message)) {
        if (this.controlMessageHandler_) {
          this.controlMessageHandler_.acceptWithResponder(message, this);
        } else {
          this.close();
        }
      } else if (this.incomingReceiver_) {
        this.incomingReceiver_.acceptWithResponder(message, this);
      } else {
        // If we receive a request expecting a response when the client is not
        // listening, then we have no choice but to tear down the pipe.
        this.close();
      }
    } else if (message.isResponse()) {
      var reader = new MessageReader(message);
      var requestID = reader.requestID;
      var completer = this.completers_.get(requestID);
      if (completer) {
        this.completers_.delete(requestID);
        completer.resolve(message);
      } else {
        console.log("Unexpected response with request ID: " + requestID);
      }
    } else {
      if (controlMessageHandler.isControlMessage(message)) {
        if (this.controlMessageHandler_) {
          var ok = this.controlMessageHandler_.accept(message);
          if (ok) return;
        }
        this.close();
      } else if (this.incomingReceiver_) {
        this.incomingReceiver_.accept(message);
      }
    }
  };

  Router.prototype.handleInvalidIncomingMessage_ = function(message, error) {
    if (!this.testingController_) {
      // TODO(yzshen): Consider notifying the embedder.
      // TODO(yzshen): This should also trigger connection error handler.
      // Consider making accept() return a boolean and let the connector deal
      // with this, as the C++ code does.
      console.log("Invalid message: " + validator.validationError[error]);

      this.close();
      return;
    }

    this.testingController_.onInvalidIncomingMessage(error);
  };

  Router.prototype.handleConnectionError_ = function(result) {
    this.completers_.forEach(function(value) {
      value.reject(result);
    });
    if (this.errorHandler_)
      this.errorHandler_();
    this.close();
  };

  // The RouterTestingController is used in unit tests. It defeats valid message
  // handling and delgates invalid message handling.

  function RouterTestingController(connector) {
    this.connector_ = connector;
    this.invalidMessageHandler_ = null;
  }

  RouterTestingController.prototype.waitForNextMessage = function() {
    this.connector_.waitForNextMessageForTesting();
  };

  RouterTestingController.prototype.setInvalidIncomingMessageHandler =
      function(callback) {
    this.invalidMessageHandler_ = callback;
  };

  RouterTestingController.prototype.onInvalidIncomingMessage =
      function(error) {
    if (this.invalidMessageHandler_)
      this.invalidMessageHandler_(error);
  };

  var exports = {};
  exports.Router = Router;
  return exports;
});
