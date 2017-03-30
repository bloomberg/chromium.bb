// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/router", [
  "mojo/public/js/connector",
  "mojo/public/js/core",
  "mojo/public/js/interface_types",
  "mojo/public/js/lib/interface_endpoint_handle",
  "mojo/public/js/lib/pipe_control_message_handler",
  "mojo/public/js/lib/pipe_control_message_proxy",
  "mojo/public/js/validator",
  "timer",
], function(connector, core, types, interfaceEndpointHandle,
      controlMessageHandler, controlMessageProxy, validator, timer) {

  var Connector = connector.Connector;
  var PipeControlMessageHandler =
      controlMessageHandler.PipeControlMessageHandler;
  var PipeControlMessageProxy = controlMessageProxy.PipeControlMessageProxy;
  var Validator = validator.Validator;
  var InterfaceEndpointHandle = interfaceEndpointHandle.InterfaceEndpointHandle;

  /**
   * The state of |endpoint|. If both the endpoint and its peer have been
   * closed, removes it from |endpoints_|.
   * @enum {string}
   */
  var EndpointStateUpdateType = {
    ENDPOINT_CLOSED: 'endpoint_closed',
    PEER_ENDPOINT_CLOSED: 'peer_endpoint_closed'
  };

  function check(condition, output) {
    if (!condition) {
      // testharness.js does not rethrow errors so the error stack needs to be
      // included as a string in the error we throw for debugging layout tests.
      throw new Error((new Error()).stack);
    }
  }

  function InterfaceEndpoint(router, interfaceId) {
    this.router_ = router;
    this.id = interfaceId;
    this.closed = false;
    this.peerClosed = false;
    this.handleCreated = false;
    this.disconnectReason = null;
    this.client = null;
  }

  InterfaceEndpoint.prototype.sendMessage = function(message) {
    message.setInterfaceId(this.id);
    return this.router_.connector_.accept(message);
  };

  function Router(handle, setInterfaceIdNamespaceBit) {
    if (!core.isHandle(handle)) {
      throw new Error("Router constructor: Not a handle");
    }
    if (setInterfaceIdNamespaceBit === undefined) {
      setInterfaceIdNamespaceBit = false;
    }

    this.connector_ = new Connector(handle);

    this.connector_.setIncomingReceiver({
        accept: this.accept.bind(this),
    });
    this.connector_.setErrorHandler({
        onError: this.onPipeConnectionError.bind(this),
    });

    this.setInterfaceIdNamespaceBit_ = setInterfaceIdNamespaceBit;
    this.controlMessageHandler_ = new PipeControlMessageHandler(this);
    this.controlMessageProxy_ = new PipeControlMessageProxy(this.connector_);
    this.nextInterfaceIdValue = 1;
    this.encounteredError_ = false;
    this.endpoints_ = new Map();
  }

  Router.prototype.attachEndpointClient = function(
      interfaceEndpointHandle, interfaceEndpointClient) {
    check(types.isValidInterfaceId(interfaceEndpointHandle.id()));
    check(interfaceEndpointClient);

    var endpoint = this.endpoints_.get(interfaceEndpointHandle.id());
    check(endpoint);
    check(!endpoint.client);
    check(!endpoint.closed);
    endpoint.client = interfaceEndpointClient;

    if (endpoint.peerClosed) {
      timer.createOneShot(0,
          endpoint.client.notifyError.bind(endpoint.client));
    }

    return endpoint;
  };

  Router.prototype.detachEndpointClient = function(
      interfaceEndpointHandle) {
    check(types.isValidInterfaceId(interfaceEndpointHandle.id()));
    var endpoint = this.endpoints_.get(interfaceEndpointHandle.id());
    check(endpoint);
    check(endpoint.client);
    check(!endpoint.closed);

    endpoint.client = null;
  };

  Router.prototype.createLocalEndpointHandle = function(
      interfaceId) {
    if (!types.isValidInterfaceId(interfaceId)) {
      return new InterfaceEndpointHandle();
    }

    var endpoint = this.endpoints_.get(interfaceId);

    if (!endpoint) {
      endpoint = new InterfaceEndpoint(this, interfaceId);
      this.endpoints_.set(interfaceId, endpoint);

      check(!endpoint.handleCreated);

      if (this.encounteredError_) {
        this.updateEndpointStateMayRemove(endpoint,
            EndpointStateUpdateType.PEER_ENDPOINT_CLOSED);
      }
    } else {
      // If the endpoint already exist, it is because we have received a
      // notification that the peer endpoint has closed.
      check(!endpoint.closed);
      check(endpoint.peerClosed);

      if (endpoint.handleCreated) {
        return new InterfaceEndpointHandle();
      }
    }

    endpoint.handleCreated = true;
    return new InterfaceEndpointHandle(interfaceId, this);
  };

  Router.prototype.accept = function(message) {
    var messageValidator = new Validator(message);
    var err = messageValidator.validateMessageHeader();

    var ok = false;
    if (err !== validator.validationError.NONE) {
      validator.reportValidationError(err);
    } else if (controlMessageHandler.isPipeControlMessage(message)) {
      ok = this.controlMessageHandler_.accept(message);
    } else {
      var interfaceId = message.getInterfaceId();
      var endpoint = this.endpoints_.get(interfaceId);
      if (!endpoint || endpoint.closed) {
        return true;
      }

      if (!endpoint.client) {
        // We need to wait until a client is attached in order to dispatch
        // further messages.
        return false;
      }
      ok = endpoint.client.handleIncomingMessage_(message);
    }

    if (!ok) {
      this.handleInvalidIncomingMessage_();
    }
    return ok;
  };

  Router.prototype.close = function() {
    this.connector_.close();
    // Closing the message pipe won't trigger connection error handler.
    // Explicitly call onPipeConnectionError() so that associated endpoints
    // will get notified.
    this.onPipeConnectionError();
  };

  Router.prototype.waitForNextMessageForTesting = function() {
    this.connector_.waitForNextMessageForTesting();
  };

  Router.prototype.handleInvalidIncomingMessage_ = function(message) {
    if (!validator.isTestingMode()) {
      // TODO(yzshen): Consider notifying the embedder.
      // TODO(yzshen): This should also trigger connection error handler.
      // Consider making accept() return a boolean and let the connector deal
      // with this, as the C++ code does.
      this.close();
      return;
    }
  };

  Router.prototype.onPeerAssociatedEndpointClosed = function(interfaceId,
      reason) {
    check(!types.isMasterInterfaceId(interfaceId) || reason);

    var endpoint = this.endpoints_.get(interfaceId);
    if (!endpoint) {
      endpoint = new InterfaceEndpoint(this, interfaceId);
      this.endpoints_.set(interfaceId, endpoint);
    }

    if (reason) {
      endpoint.disconnectReason = reason;
    }

    if (!endpoint.peerClosed) {
      if (endpoint.client) {
        timer.createOneShot(0,
            endpoint.client.notifyError.bind(endpoint.client, reason));
      }
      this.updateEndpointStateMayRemove(endpoint,
          EndpointStateUpdateType.PEER_ENDPOINT_CLOSED);
    }
    return true;
  };

  Router.prototype.onPipeConnectionError = function() {
    this.encounteredError_ = true;

    for (var endpoint of this.endpoints_.values()) {
      if (endpoint.client) {
        timer.createOneShot(0,
            endpoint.client.notifyError.bind(endpoint.client,
              endpoint.disconnectReason));
      }
      this.updateEndpointStateMayRemove(endpoint,
          EndpointStateUpdateType.PEER_ENDPOINT_CLOSED);
    }
  };

  Router.prototype.closeEndpointHandle = function(interfaceId, reason) {
    if (!types.isValidInterfaceId(interfaceId)) {
      return;
    }
    var endpoint = this.endpoints_.get(interfaceId);
    check(endpoint);
    check(!endpoint.client);
    check(!endpoint.closed);

    this.updateEndpointStateMayRemove(endpoint,
        EndpointStateUpdateType.ENDPOINT_CLOSED);

    if (!types.isMasterInterfaceId(interfaceId) || reason) {
      this.controlMessageProxy_.notifyPeerEndpointClosed(interfaceId, reason);
    }
  };

  Router.prototype.updateEndpointStateMayRemove = function(endpoint,
      endpointStateUpdateType) {
    if (endpointStateUpdateType === EndpointStateUpdateType.ENDPOINT_CLOSED) {
      endpoint.closed = true;
    } else {
      endpoint.peerClosed = true;
    }
    if (endpoint.closed && endpoint.peerClosed) {
      this.endpoints_.delete(endpoint.id);
    }
  };

  var exports = {};
  exports.Router = Router;
  return exports;
});
