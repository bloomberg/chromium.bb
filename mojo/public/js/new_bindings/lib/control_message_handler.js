// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  function validateControlRequestWithResponse(message) {
    var messageValidator = new internal.Validator(message);
    var error = messageValidator.validateMessageIsRequestExpectingResponse();
    if (error !== internal.validationError.NONE) {
      throw error;
    }

    if (message.getName() != mojo.interface_control2.kRunMessageId) {
      throw new Error("Control message name is not kRunMessageId");
    }

    // Validate payload.
    error = mojo.interface_control2.RunMessageParams.validate(messageValidator,
        message.getHeaderNumBytes());
    if (error != internal.validationError.NONE) {
      throw error;
    }
  }

  function validateControlRequestWithoutResponse(message) {
    var messageValidator = new internal.Validator(message);
    var error = messageValidator.validateMessageIsRequestWithoutResponse();
    if (error != internal.validationError.NONE) {
      throw error;
    }

    if (message.getName() != mojo.interface_control2.kRunOrClosePipeMessageId) {
      throw new Error("Control message name is not kRunOrClosePipeMessageId");
    }

    // Validate payload.
    error = mojo.interface_control2.RunOrClosePipeMessageParams.validate(
        messageValidator, message.getHeaderNumBytes());
    if (error != internal.validationError.NONE) {
      throw error;
    }
  }

  function runOrClosePipe(message, interface_version) {
    var reader = new internal.MessageReader(message);
    var runOrClosePipeMessageParams = reader.decodeStruct(
        mojo.interface_control2.RunOrClosePipeMessageParams);
    return interface_version >=
        runOrClosePipeMessageParams.input.require_version.version;
  }

  function run(message, responder, interface_version) {
    var reader = new internal.MessageReader(message);
    var runMessageParams =
        reader.decodeStruct(mojo.interface_control2.RunMessageParams);
    var runOutput = null;

    if (runMessageParams.input.query_version) {
      runOutput = new mojo.interface_control2.RunOutput();
      runOutput.query_version_result = new
          mojo.interface_control2.QueryVersionResult(
              {'version': interface_version});
    }

    var runResponseMessageParams = new
        mojo.interface_control2.RunResponseMessageParams();
    runResponseMessageParams.output = runOutput;

    var messageName = mojo.interface_control2.kRunMessageId;
    var payloadSize =
        mojo.interface_control2.RunResponseMessageParams.encodedSize;
    var requestID = reader.requestID;
    var builder = new internal.MessageWithRequestIDBuilder(messageName,
        payloadSize, internal.kMessageIsResponse, requestID);
    builder.encodeStruct(mojo.interface_control2.RunResponseMessageParams,
                         runResponseMessageParams);
    responder.accept(builder.finish());
    return true;
  }

  function isInterfaceControlMessage(message) {
    return message.getName() == mojo.interface_control2.kRunMessageId ||
           message.getName() ==
               mojo.interface_control2.kRunOrClosePipeMessageId;
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

  internal.ControlMessageHandler = ControlMessageHandler;
  internal.isInterfaceControlMessage = isInterfaceControlMessage;
})();
