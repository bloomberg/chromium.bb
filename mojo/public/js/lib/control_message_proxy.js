// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/lib/control_message_proxy", [
  "mojo/public/interfaces/bindings/interface_control_messages.mojom",
  "mojo/public/js/codec",
  "mojo/public/js/validator",
], function(controlMessages, codec, validator) {

  var Validator = validator.Validator;

  function sendRunOrClosePipeMessage(receiver, runOrClosePipeMessageParams) {
    var messageName = controlMessages.kRunOrClosePipeMessageId;
    var payloadSize = controlMessages.RunOrClosePipeMessageParams.encodedSize;
    var builder = new codec.MessageBuilder(messageName, payloadSize);
    builder.encodeStruct(controlMessages.RunOrClosePipeMessageParams,
                         runOrClosePipeMessageParams);
    var message = builder.finish();
    receiver.accept(message);
  }

  function validateControlResponse(message) {
    var messageValidator = new Validator(message);
    var error = messageValidator.validateMessageIsResponse();
    if (error != validator.validationError.NONE) {
      throw error;
    }

    if (message.getName() != controlMessages.kRunMessageId) {
      throw new Error("Control message name is not kRunMessageId");
    }

    // Validate payload.
    error = controlMessages.RunResponseMessageParams.validate(
        messageValidator, message.getHeaderNumBytes());
    if (error != validator.validationError.NONE) {
      throw error;
    }
  }

  function acceptRunResponse(message) {
    validateControlResponse(message);

    var reader = new codec.MessageReader(message);
    var runResponseMessageParams = reader.decodeStruct(
        controlMessages.RunResponseMessageParams);

    return Promise.resolve(runResponseMessageParams);
  }

 /**
  * Sends the given run message through the receiver.
  * Accepts the response message from the receiver and decodes the message
  * struct to RunResponseMessageParams.
  *
  * @param  {Router} receiver.
  * @param  {RunMessageParams} runMessageParams to be sent via a message.
  * @return {Promise} that resolves to a RunResponseMessageParams.
  */
  function sendRunMessage(receiver, runMessageParams) {
    var messageName = controlMessages.kRunMessageId;
    var payloadSize = controlMessages.RunMessageParams.encodedSize;
    // |requestID| is set to 0, but is later properly set by Router.
    var builder = new codec.MessageWithRequestIDBuilder(messageName,
        payloadSize, codec.kMessageExpectsResponse, 0);
    builder.encodeStruct(controlMessages.RunMessageParams, runMessageParams);
    var message = builder.finish();

    return receiver.acceptAndExpectResponse(message).then(acceptRunResponse);
  }

  function ControlMessageProxy(receiver) {
    this.receiver = receiver;
  }

  ControlMessageProxy.prototype.queryVersion = function() {
    var runMessageParams = new controlMessages.RunMessageParams();
    runMessageParams.input = new controlMessages.RunInput();
    runMessageParams.input.query_version = new controlMessages.QueryVersion();

    return sendRunMessage(this.receiver, runMessageParams).then(function(
        runResponseMessageParams) {
      return runResponseMessageParams.output.query_version_result.version;
    });
  };

  ControlMessageProxy.prototype.requireVersion = function(version) {
    var runOrClosePipeMessageParams = new
        controlMessages.RunOrClosePipeMessageParams();
    runOrClosePipeMessageParams.input = new
        controlMessages.RunOrClosePipeInput();
    runOrClosePipeMessageParams.input.require_version = new
        controlMessages.RequireVersion({'version': version});
    sendRunOrClosePipeMessage(this.receiver, runOrClosePipeMessageParams);
  };

  var exports = {};
  exports.ControlMessageProxy = ControlMessageProxy;

  return exports;
});
