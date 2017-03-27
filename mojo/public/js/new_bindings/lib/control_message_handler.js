// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/lib/control_message_handler", [
  "mojo/public/js/codec",
  "mojo/public/interfaces/bindings/interface_control_messages.mojom",
  "mojo/public/js/validator",
], function(codec, controlMessages, validator) {

  var Validator = validator.Validator;

  function validateControlRequestWithResponse(message) {
    var messageValidator = new Validator(message);
    var error = messageValidator.validateMessageIsRequestExpectingResponse();
    if (error !== validator.validationError.NONE) {
      throw error;
    }

    if (message.getName() != controlMessages.kRunMessageId) {
      throw new Error("Control message name is not kRunMessageId");
    }

    // Validate payload.
    error = controlMessages.RunMessageParams.validate(messageValidator,
        message.getHeaderNumBytes());
    if (error != validator.validationError.NONE) {
      throw error;
    }
  }

  function validateControlRequestWithoutResponse(message) {
    var messageValidator = new Validator(message);
    var error = messageValidator.validateMessageIsRequestWithoutResponse();
    if (error != validator.validationError.NONE) {
      throw error;
    }

    if (message.getName() != controlMessages.kRunOrClosePipeMessageId) {
      throw new Error("Control message name is not kRunOrClosePipeMessageId");
    }

    // Validate payload.
    error = controlMessages.RunOrClosePipeMessageParams.validate(
        messageValidator, message.getHeaderNumBytes());
    if (error != validator.validationError.NONE) {
      throw error;
    }
  }

  function runOrClosePipe(message, interface_version) {
    var reader = new codec.MessageReader(message);
    var runOrClosePipeMessageParams = reader.decodeStruct(
        controlMessages.RunOrClosePipeMessageParams);
    return interface_version >=
        runOrClosePipeMessageParams.input.require_version.version;
  }

  function run(message, responder, interface_version) {
    var reader = new codec.MessageReader(message);
    var runMessageParams =
        reader.decodeStruct(controlMessages.RunMessageParams);
    var runOutput = null;

    if (runMessageParams.input.query_version) {
      runOutput = new controlMessages.RunOutput();
      runOutput.query_version_result = new
          controlMessages.QueryVersionResult({'version': interface_version});
    }

    var runResponseMessageParams = new
        controlMessages.RunResponseMessageParams();
    runResponseMessageParams.output = runOutput;

    var messageName = controlMessages.kRunMessageId;
    var payloadSize = controlMessages.RunResponseMessageParams.encodedSize;
    var requestID = reader.requestID;
    var builder = new codec.MessageWithRequestIDBuilder(messageName,
        payloadSize, codec.kMessageIsResponse, requestID);
    builder.encodeStruct(controlMessages.RunResponseMessageParams,
                         runResponseMessageParams);
    responder.accept(builder.finish());
    return true;
  }

  function isControlMessage(message) {
    return message.getName() == controlMessages.kRunMessageId ||
           message.getName() == controlMessages.kRunOrClosePipeMessageId;
  }

  function ControlMessageHandler(interface_version) {
    this.interface_version = interface_version;
  }

  ControlMessageHandler.prototype.accept = function(message) {
    validateControlRequestWithoutResponse(message);
    return runOrClosePipe(message, this.interface_version);
  };

  ControlMessageHandler.prototype.acceptWithResponder = function(message,
      responder) {
    validateControlRequestWithResponse(message);
    return run(message, responder, this.interface_version);
  };

  var exports = {};
  exports.ControlMessageHandler = ControlMessageHandler;
  exports.isControlMessage = isControlMessage;

  return exports;
});
