// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/lib/pipe_control_message_handler", [
  "mojo/public/interfaces/bindings/pipe_control_messages.mojom",
  "mojo/public/js/codec",
  "mojo/public/js/interface_types",
  "mojo/public/js/validator",
], function(pipeControlMessages, codec, types, validator) {

  var Validator = validator.Validator;

  function validateControlRequestWithoutResponse(message) {
    var messageValidator = new Validator(message);
    var error = messageValidator.validateMessageIsRequestWithoutResponse();
    if (error != validator.validationError.NONE) {
      throw error;
    }

    if (message.getName() != pipeControlMessages.kRunOrClosePipeMessageId) {
      throw new Error("Control message name is not kRunOrClosePipeMessageId");
    }

    // Validate payload.
    error = pipeControlMessages.RunOrClosePipeMessageParams.validate(
        messageValidator, message.getHeaderNumBytes());
    if (error != validator.validationError.NONE) {
      throw error;
    }
  }

  function runOrClosePipe(message, delegate) {
    var reader = new codec.MessageReader(message);
    var runOrClosePipeMessageParams = reader.decodeStruct(
        pipeControlMessages.RunOrClosePipeMessageParams);
    var event = runOrClosePipeMessageParams.input
        .peer_associated_endpoint_closed_event;
    return delegate.onPeerAssociatedEndpointClosed(event.id,
        event.disconnect_reason);
  }

  function isPipeControlMessage(message) {
    return !types.isValidInterfaceId(message.getInterfaceId());
  }

  function PipeControlMessageHandler(delegate) {
    this.delegate_ = delegate;
  }

  PipeControlMessageHandler.prototype.accept = function(message) {
    validateControlRequestWithoutResponse(message);
    return runOrClosePipe(message, this.delegate_);
  };

  var exports = {};
  exports.PipeControlMessageHandler = PipeControlMessageHandler;
  exports.isPipeControlMessage = isPipeControlMessage;

  return exports;
});
