// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/lib/pipe_control_message_proxy", [
  "mojo/public/interfaces/bindings/pipe_control_messages.mojom",
  "mojo/public/js/codec",
  "mojo/public/js/interface_types",
], function(pipeControlMessages, codec, types) {

  function constructRunOrClosePipeMessage(runOrClosePipeInput) {
    var runOrClosePipeMessageParams = new
        pipeControlMessages.RunOrClosePipeMessageParams();
    runOrClosePipeMessageParams.input = runOrClosePipeInput;

    var messageName = pipeControlMessages.kRunOrClosePipeMessageId;
    var payloadSize =
        pipeControlMessages.RunOrClosePipeMessageParams.encodedSize;

    var builder = new codec.MessageBuilder(messageName, payloadSize);
    builder.encodeStruct(pipeControlMessages.RunOrClosePipeMessageParams,
                         runOrClosePipeMessageParams);
    var message = builder.finish();
    message.setInterfaceId(types.kInvalidInterfaceId);
    return message;
  }

  function PipeControlMessageProxy(receiver) {
    this.receiver_ = receiver;
  }

  PipeControlMessageProxy.prototype.notifyPeerEndpointClosed = function(
      interfaceId, reason) {
    var message = this.constructPeerEndpointClosedMessage(interfaceId, reason);
    this.receiver_.accept(message);
  };

  PipeControlMessageProxy.prototype.constructPeerEndpointClosedMessage =
      function(interfaceId, reason) {
    var event = new pipeControlMessages.PeerAssociatedEndpointClosedEvent();
    event.id = interfaceId;
    if (reason) {
      event.disconnect_reason = new pipeControlMessages.DisconnectReason({
          custom_reason: reason.custom_reason,
          description: reason.description});
    }
    var runOrClosePipeInput = new pipeControlMessages.RunOrClosePipeInput();
    runOrClosePipeInput.peer_associated_endpoint_closed_event = event;
    return constructRunOrClosePipeMessage(runOrClosePipeInput);
  };

  var exports = {};
  exports.PipeControlMessageProxy = PipeControlMessageProxy;

  return exports;
});
