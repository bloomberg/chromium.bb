// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  function constructRunOrClosePipeMessage(runOrClosePipeInput) {
    var runOrClosePipeMessageParams = new
        mojo.interfaceControl2.RunOrClosePipeMessageParams();
    runOrClosePipeMessageParams.input = runOrClosePipeInput;

    var messageName = mojo.interfaceControl2.kRunOrClosePipeMessageId;
    var payloadSize =
        mojo.interfaceControl2.RunOrClosePipeMessageParams.encodedSize;
    var builder = new internal.MessageV0Builder(messageName, payloadSize);
    builder.encodeStruct(mojo.interfaceControl2.RunOrClosePipeMessageParams,
                         runOrClosePipeMessageParams);
    var message = builder.finish();
    return message;
  }

  function validateControlResponse(message) {
    var messageValidator = new internal.Validator(message);
    var error = messageValidator.validateMessageIsResponse();
    if (error != internal.validationError.NONE) {
      throw error;
    }

    if (message.getName() != mojo.interfaceControl2.kRunMessageId) {
      throw new Error("Control message name is not kRunMessageId");
    }

    // Validate payload.
    error = mojo.interfaceControl2.RunResponseMessageParams.validate(
        messageValidator, message.getHeaderNumBytes());
    if (error != internal.validationError.NONE) {
      throw error;
    }
  }

  function acceptRunResponse(message) {
    validateControlResponse(message);

    var reader = new internal.MessageReader(message);
    var runResponseMessageParams = reader.decodeStruct(
        mojo.interfaceControl2.RunResponseMessageParams);

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
    var messageName = mojo.interfaceControl2.kRunMessageId;
    var payloadSize = mojo.interfaceControl2.RunMessageParams.encodedSize;
    // |requestID| is set to 0, but is later properly set by Router.
    var builder = new internal.MessageV1Builder(messageName,
        payloadSize, internal.kMessageExpectsResponse, 0);
    builder.encodeStruct(mojo.interfaceControl2.RunMessageParams,
                         runMessageParams);
    var message = builder.finish();

    return receiver.acceptAndExpectResponse(message).then(acceptRunResponse);
  }

  function ControlMessageProxy(receiver) {
    this.receiver_ = receiver;
  }

  ControlMessageProxy.prototype.queryVersion = function() {
    var runMessageParams = new mojo.interfaceControl2.RunMessageParams();
    runMessageParams.input = new mojo.interfaceControl2.RunInput();
    runMessageParams.input.queryVersion =
        new mojo.interfaceControl2.QueryVersion();

    return sendRunMessage(this.receiver_, runMessageParams).then(function(
        runResponseMessageParams) {
      return runResponseMessageParams.output.queryVersionResult.version;
    });
  };

  ControlMessageProxy.prototype.requireVersion = function(version) {
    var runOrClosePipeInput = new mojo.interfaceControl2.RunOrClosePipeInput();
    runOrClosePipeInput.requireVersion =
        new mojo.interfaceControl2.RequireVersion({'version': version});
    var message = constructRunOrClosePipeMessage(runOrClosePipeInput);
    this.receiver_.accept(message);
  };

  internal.ControlMessageProxy = ControlMessageProxy;
})();
