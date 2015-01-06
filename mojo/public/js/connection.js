// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/connection", [
  "mojo/public/js/bindings",
  "mojo/public/js/connector",
  "mojo/public/js/core",
  "mojo/public/js/router",
], function(bindings, connector, core, router) {

  var Router = router.Router;
  var EmptyProxy = bindings.EmptyProxy;
  var EmptyStub = bindings.EmptyStub;
  var ProxyBindings = bindings.ProxyBindings;
  var StubBindings = bindings.StubBindings;
  var TestConnector = connector.TestConnector;
  var TestRouter = router.TestRouter;

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
    var routerClass = routerFactory || Router;
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
                    TestRouter,
                    TestConnector);
  }

  TestConnection.prototype = Object.create(Connection.prototype);

  function createOpenConnection(
      messagePipeHandle, client, localInterface, remoteInterface) {
    var stubClass = (localInterface && localInterface.stubClass) || EmptyStub;
    var proxyClass =
        (remoteInterface && remoteInterface.proxyClass) || EmptyProxy;
    var proxy = new proxyClass;
    var stub = new stubClass;
    var router = new Router(messagePipeHandle);
    var connection = new BaseConnection(stub, proxy, router);

    ProxyBindings(proxy).connection = connection;
    ProxyBindings(proxy).local = connection.local;
    StubBindings(stub).connection = connection;
    StubBindings(proxy).remote = connection.remote;

    var clientImpl = client instanceof Function ? client(proxy) : client;
    if (clientImpl)
      StubBindings(stub).delegate = clientImpl;

    return connection;
  }

  // Return a message pipe handle.
  function bindProxyClient(clientImpl, localInterface, remoteInterface) {
    var messagePipe = core.createMessagePipe();
    if (messagePipe.result != core.RESULT_OK)
      throw new Error("createMessagePipe failed " + messagePipe.result);

    createOpenConnection(
      messagePipe.handle0, clientImpl, localInterface, remoteInterface);
    return messagePipe.handle1;
  }

  // Return a proxy.
  function bindProxyHandle(proxyHandle, localInterface, remoteInterface) {
    if (!core.isHandle(proxyHandle))
      throw new Error("Not a handle " + proxyHandle);

    var connection = createOpenConnection(
        proxyHandle, undefined, localInterface, remoteInterface);
    return connection.remote;
  }

  var exports = {};
  exports.Connection = Connection;
  exports.TestConnection = TestConnection;
  exports.bindProxyHandle = bindProxyHandle;
  exports.bindProxyClient = bindProxyClient;
  return exports;
});
