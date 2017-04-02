// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  function sendRunOrClosePipeMessage(receiver, runOrClosePipeMessageParams) {
    var messageName = mojo.interface_control2.kRunOrClosePipeMessageId;
    var payloadSize =
        mojo.interface_control2.RunOrClosePipeMessageParams.encodedSize;
    var builder = new internal.MessageBuilder(messageName, payloadSize);
    builder.encodeStruct(mojo.interface_control2.RunOrClosePipeMessageParams,
                         runOrClosePipeMessageParams);
    var message = builder.finish();
    receiver.accept(message);
  }

  function validateControlResponse(message) {
    var messageValidator = new internal.Validator(message);
    var error = messageValidator.validateMessageIsResponse();
    if (error != internal.validationError.NONE) {
      throw error;
    }

    if (message.getName() != mojo.interface_control2.kRunMessageId) {
      throw new Error("Control message name is not kRunMessageId");
    }

    // Validate payload.
    error = mojo.interface_control2.RunResponseMessageParams.validate(
        messageValidator, message.getHeaderNumBytes());
    if (error != internal.validationError.NONE) {
      throw error;
    }
  }

  function acceptRunResponse(message) {
    validateControlResponse(message);

    var reader = new internal.MessageReader(message);
    var runResponseMessageParams = reader.decodeStruct(
        mojo.interface_control2.RunResponseMessageParams);

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
    var messageName = mojo.interface_control2.kRunMessageId;
    var payloadSize = mojo.interface_control2.RunMessageParams.encodedSize;
    // |requestID| is set to 0, but is later properly set by Router.
    var builder = new internal.MessageWithRequestIDBuilder(messageName,
        payloadSize, internal.kMessageExpectsResponse, 0);
    builder.encodeStruct(mojo.interface_control2.RunMessageParams,
                         runMessageParams);
    var message = builder.finish();

    return receiver.acceptAndExpectResponse(message).then(acceptRunResponse);
  }

  function ControlMessageProxy(receiver) {
    this.receiver = receiver;
  }

  ControlMessageProxy.prototype.queryVersion = function() {
    var runMessageParams = new mojo.interface_control2.RunMessageParams();
    runMessageParams.input = new mojo.interface_control2.RunInput();
    runMessageParams.input.query_version =
        new mojo.interface_control2.QueryVersion();

    return sendRunMessage(this.receiver, runMessageParams).then(function(
        runResponseMessageParams) {
      return runResponseMessageParams.output.query_version_result.version;
    });
  };

  ControlMessageProxy.prototype.requireVersion = function(version) {
    var runOrClosePipeMessageParams = new
        mojo.interface_control2.RunOrClosePipeMessageParams();
    runOrClosePipeMessageParams.input = new
        mojo.interface_control2.RunOrClosePipeInput();
    runOrClosePipeMessageParams.input.require_version = new
        mojo.interface_control2.RequireVersion({'version': version});
    sendRunOrClosePipeMessage(this.receiver, runOrClosePipeMessageParams);
  };

  internal.ControlMessageProxy = ControlMessageProxy;
})();
