// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/connection", [
  "mojo/public/js/connector",
  "mojo/public/js/core",
  "mojo/public/js/router",
], function(connector, core, routerModule) {

  // TODO(hansmuller): the proxy connection_ property should connection$
  // TODO(hansmuller): the proxy receiver_ property should be receiver$

  function BaseConnection(localStub, remoteProxy, router) {
    this.router_ = router;
    this.local = localStub;
    this.remote = remoteProxy;

    this.router_.setIncomingReceiver(localStub);
    if (this.remote)
      this.remote.receiver_ = router;

    // Validate incoming messages: remote responses and local requests.
    var validateRequest = localStub && localStub.validator;
    var validateResponse = remoteProxy && remoteProxy.validator;
    var payloadValidators = [];
    if (validateRequest)
      payloadValidators.push(validateRequest);
    if (validateResponse)
      payloadValidators.push(validateResponse);
    this.router_.setPayloadValidators(payloadValidators);
  }

  BaseConnection.prototype.close = function() {
    this.router_.close();
    this.router_ = null;
    this.local = null;
    this.remote = null;
  };

  BaseConnection.prototype.encounteredError = function() {
    return this.router_.encounteredError();
  };

  function Connection(
      handle, localFactory, remoteFactory, routerFactory, connectorFactory) {
    var routerClass = routerFactory || routerModule.Router;
    var router = new routerClass(handle, connectorFactory);
    var remoteProxy = remoteFactory && new remoteFactory(router);
    var localStub = localFactory && new localFactory(remoteProxy);
    BaseConnection.call(this, localStub, remoteProxy, router);
  }

  Connection.prototype = Object.create(BaseConnection.prototype);

  // The TestConnection subclass is only intended to be used in unit tests.

  function TestConnection(handle, localFactory, remoteFactory) {
    Connection.call(this,
                    handle,
                    localFactory,
                    remoteFactory,
                    routerModule.TestRouter,
                    connector.TestConnector);
  }

  TestConnection.prototype = Object.create(Connection.prototype);

  function createOpenConnection(stub, proxy) {
    var messagePipe = core.createMessagePipe();
    // TODO(hansmuller): Check messagePipe.result.
    var router = new routerModule.Router(messagePipe.handle0);
    var connection = new BaseConnection(stub, proxy, router);
    connection.messagePipeHandle = messagePipe.handle1;
    return connection;
  }

  function getProxyConnection(proxy, proxyInterface) {
    if (!proxy.connection_) {
      var stub = proxyInterface.client && new proxyInterface.client.stubClass;
      proxy.connection_ = createOpenConnection(stub, proxy);
    }
    return proxy.connection_;
  }

  function getStubConnection(stub, proxyInterface) {
    if (!stub.connection_) {
      var proxy = proxyInterface.client && new proxyInterface.client.proxyClass;
      stub.connection_ = createOpenConnection(stub, proxy);
    }
    return stub.connection_;
  }

  function initProxyInstance(proxy, proxyInterface, receiver) {
    if (proxyInterface.client) {
      Object.defineProperty(proxy, 'client$', {
        get: function() {
          var connection = getProxyConnection(proxy, proxyInterface);
          return connection.local && connection.local.delegate$
        },
        set: function(value) {
          var connection = getProxyConnection(proxy, proxyInterface);
          if (connection.local)
            connection.local.delegate$ = value;
        }
      });
    }
    if (receiver instanceof routerModule.Router) {
      // TODO(hansmuller): Temporary, for Chrome backwards compatibility.
      proxy.receiver_ = receiver;
    } else if (receiver) {
      var router = new routerModule.Router(receiver);
      var stub = proxyInterface.client && new proxyInterface.client.stubClass;
      proxy.connection_  = new BaseConnection(stub, proxy, router);
      proxy.connection_.messagePipeHandle = receiver;
    }
  }

  var exports = {};
  exports.Connection = Connection;
  exports.TestConnection = TestConnection;
  exports.getProxyConnection = getProxyConnection;
  exports.getStubConnection = getStubConnection;
  exports.initProxyInstance = initProxyInstance;
  return exports;
});
