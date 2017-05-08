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
    // |cachedMessageData| caches infomation about a message, so it can be
    // processed later if a client is not yet attached to the target endpoint.
    this.cachedMessageData = null;
    this.controlMessageHandler_ = new PipeControlMessageHandler(this);
    this.controlMessageProxy_ = new PipeControlMessageProxy(this.connector_);
    this.nextInterfaceIdValue_ = 1;
    this.encounteredError_ = false;
    this.endpoints_ = new Map();
  }

  Router.prototype.associateInterface = function(handleToSend) {
    if (!handleToSend.pendingAssociation()) {
      return types.kInvalidInterfaceId;
    }

    var id = 0;
    do {
      if (this.nextInterfaceIdValue_ >= types.kInterfaceIdNamespaceMask) {
        this.nextInterfaceIdValue_ = 1;
      }
      id = this.nextInterfaceIdValue_++;
      if (this.setInterfaceIdNamespaceBit_) {
        id += types.kInterfaceIdNamespaceMask;
      }
    } while (this.endpoints_.has(id));

    var endpoint = new InterfaceEndpoint(this, id);
    this.endpoints_.set(id, endpoint);
    if (this.encounteredError_) {
      this.updateEndpointStateMayRemove(endpoint,
          EndpointStateUpdateType.PEER_ENDPOINT_CLOSED);
    }
    endpoint.handleCreated = true;

    if (!handleToSend.notifyAssociation(id, this)) {
      // The peer handle of |handleToSend|, which is supposed to join this
      // associated group, has been closed.
      this.updateEndpointStateMayRemove(endpoint,
          EndpointStateUpdateType.ENDPOINT_CLOSED);

      pipeControlMessageproxy.notifyPeerEndpointClosed(id,
          handleToSend.disconnectReason());
    }

    return id;
  };

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

    if (this.cachedMessageData && interfaceEndpointHandle.id() ===
        this.cachedMessageData.message.getInterfaceId()) {
      timer.createOneShot(0, (function() {
        if (!this.cachedMessageData) {
          return;
        }

        var targetEndpoint = this.endpoints_.get(
            this.cachedMessageData.message.getInterfaceId());
        // Check that the target endpoint's client still exists.
        if (targetEndpoint && targetEndpoint.client) {
          var message = this.cachedMessageData.message;
          var messageValidator = this.cachedMessageData.messageValidator;
          this.cachedMessageData = null;
          this.connector_.resumeIncomingMethodCallProcessing();
          var ok = endpoint.client.handleIncomingMessage(message,
              messageValidator);

          // Handle invalid cached incoming message.
          if (!validator.isTestingMode() && !ok) {
            this.connector_.handleError(true, true);
          }
        }
      }).bind(this));
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
    } else if (message.deserializeAssociatedEndpointHandles(this)) {
      if (controlMessageHandler.isPipeControlMessage(message)) {
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
          this.cachedMessageData = {message: message,
              messageValidator: messageValidator};
          this.connector_.pauseIncomingMethodCallProcessing();
          return true;
        }
        ok = endpoint.client.handleIncomingMessage(message, messageValidator);
      }
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

    if (this.cachedMessageData && interfaceId ===
        this.cachedMessageData.message.getInterfaceId()) {
      this.cachedMessageData = null;
      this.connector_.resumeIncomingMethodCallProcessing();
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
