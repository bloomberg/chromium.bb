// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/connection", [
  "mojo/public/js/connector",
  "mojo/public/js/core",
  "mojo/public/js/router",
], function(connector, core, router) {

  var Router = router.Router;
  var TestConnector = connector.TestConnector;
  var TestRouter = router.TestRouter;

  var kProxyProperties = Symbol("proxyProperties");
  var kStubProperties = Symbol("stubProperties");

  // Public proxy class properties that are managed at runtime by the JS
  // bindings. See ProxyBindings below.
  function ProxyProperties(receiver) {
    this.receiver = receiver;
  }

  // TODO(hansmuller): remove then after 'Client=' has been removed from Mojom.
  ProxyProperties.prototype.getLocalDelegate = function() {
    return this.local && StubBindings(this.local).delegate;
  }

  // TODO(hansmuller): remove then after 'Client=' has been removed from Mojom.
  ProxyProperties.prototype.setLocalDelegate = function(impl) {
    if (this.local)
      StubBindings(this.local).delegate = impl;
    else
      throw new Error("no stub object");
  }

  ProxyProperties.prototype.close = function() {
    this.connection.close();
  }

  // Public stub class properties that are managed at runtime by the JS
  // bindings. See StubBindings below.
  function StubProperties(delegate) {
    this.delegate = delegate;
  }

  StubProperties.prototype.close = function() {
    this.connection.close();
  }

  // The base class for generated proxy classes.
  function ProxyBase(receiver) {
    this[kProxyProperties] = new ProxyProperties(receiver);

    // TODO(hansmuller): Temporary, for Chrome backwards compatibility.
    if (receiver instanceof Router)
      this.receiver_ = receiver;
  }

  // The base class for generated stub classes.
  function StubBase(delegate) {
    this[kStubProperties] = new StubProperties(delegate);
  }

  // TODO(hansmuller): remove everything except the connection property doc
  // after 'Client=' has been removed from Mojom.

  // Provides access to properties added to a proxy object without risking
  // Mojo interface name collisions. Unless otherwise specified, the initial
  // value of all properties is undefined.
  //
  // ProxyBindings(proxy).connection - The Connection object that links the
  //   proxy for a remote Mojo service to an optional local stub for a local
  //   service. The value of ProxyBindings(proxy).connection.remote == proxy.
  //
  // ProxyBindings(proxy).local  - The "local" stub object whose delegate
  //   implements the proxy's Mojo client interface.
  //
  // ProxyBindings(proxy).setLocalDelegate(impl) - Sets the implementation
  //   delegate of the proxy's client stub object. This is just shorthand
  //   for |StubBindings(ProxyBindings(proxy).local).delegate = impl|.
  //
  // ProxyBindings(proxy).getLocalDelegate() - Returns the implementation
  //   delegate of the proxy's client stub object. This is just shorthand
  //   for |StubBindings(ProxyBindings(proxy).local).delegate|.

  function ProxyBindings(proxy) {
    return (proxy instanceof ProxyBase) ? proxy[kProxyProperties] : proxy;
  }

  // TODO(hansmuller): remove the remote doc after 'Client=' has been
  // removed from Mojom.

  // Provides access to properties added to a stub object without risking
  // Mojo interface name collisions. Unless otherwise specified, the initial
  // value of all properties is undefined.
  //
  // StubBindings(stub).delegate - The optional implementation delegate for
  //  the Mojo interface stub.
  //
  // StubBindings(stub).connection - The Connection object that links an
  //   optional proxy for a remote service to this stub. The value of
  //   StubBindings(stub).connection.local == stub.
  //
  // StubBindings(stub).remote - A proxy for the the stub's Mojo client
  //   service.

  function StubBindings(stub) {
    return stub instanceof StubBase ?  stub[kStubProperties] : stub;
  }

  // TODO(hansmuller): the proxy receiver_ property should be receiver$

  function BaseConnection(localStub, remoteProxy, router) {
    this.router_ = router;
    this.local = localStub;
    this.remote = remoteProxy;

    this.router_.setIncomingReceiver(localStub);
    this.router_.setErrorHandler(function() {
      if (StubBindings(this.local) &&
          StubBindings(this.local).connectionErrorHandler)
        StubBindings(this.local).connectionErrorHandler();
    }.bind(this));
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

  // Return a handle for a message pipe that's connected to a proxy
  // for remoteInterface. Used by generated code for outgoing interface&
  // (request) parameters: the caller is given the generated proxy via
  // |proxyCallback(proxy)| and the generated code sends the handle
  // returned by this function.
  function bindProxy(proxyCallback, remoteInterface) {
    var messagePipe = core.createMessagePipe();
    if (messagePipe.result != core.RESULT_OK)
      throw new Error("createMessagePipe failed " + messagePipe.result);

    var proxy = new remoteInterface.proxyClass;
    var router = new Router(messagePipe.handle0);
    var connection = new BaseConnection(undefined, proxy, router);
    ProxyBindings(proxy).connection = connection;
    if (proxyCallback)
      proxyCallback(proxy);

    return messagePipe.handle1;
  }

  // Return a handle and proxy for a message pipe that's connected to a proxy
  // for remoteInterface. Used by generated code for outgoing interface&
  // (request) parameters
  function getProxy(remoteInterface) {
    var messagePipe = core.createMessagePipe();
    if (messagePipe.result != core.RESULT_OK)
      throw new Error("createMessagePipe failed " + messagePipe.result);

    var proxy = new remoteInterface.proxyClass;
    var router = new Router(messagePipe.handle0);
    var connection = new BaseConnection(undefined, proxy, router);
    ProxyBindings(proxy).connection = connection;

    return {
      requestHandle: messagePipe.handle1,
      proxy: proxy
    };
  }

  // Return a handle for a message pipe that's connected to a stub for
  // localInterface. Used by generated code for outgoing interface
  // parameters: the caller  is given the generated stub via
  // |stubCallback(stub)| and the generated code sends the handle
  // returned by this function. The caller is responsible for managing
  // the lifetime of the stub and for setting it's implementation
  // delegate with: StubBindings(stub).delegate = myImpl;
  function bindImpl(stubCallback, localInterface) {
    var messagePipe = core.createMessagePipe();
    if (messagePipe.result != core.RESULT_OK)
      throw new Error("createMessagePipe failed " + messagePipe.result);

    var stub = new localInterface.stubClass;
    var router = new Router(messagePipe.handle0);
    var connection = new BaseConnection(stub, undefined, router);
    StubBindings(stub).connection = connection;
    if (stubCallback)
      stubCallback(stub);

    return messagePipe.handle1;
  }

  // Return a remoteInterface proxy for handle. Used by generated code
  // for converting incoming interface parameters to proxies.
  function bindHandleToProxy(handle, remoteInterface) {
    if (!core.isHandle(handle))
      throw new Error("Not a handle " + handle);

    var proxy = new remoteInterface.proxyClass;
    var router = new Router(handle);
    var connection = new BaseConnection(undefined, proxy, router);
    ProxyBindings(proxy).connection = connection;
    return proxy;
  }

  // Return a localInterface stub for handle. Used by generated code
  // for converting incoming interface& request parameters to localInterface
  // stubs. The caller can specify the stub's implementation of localInterface
  // like this: StubBindings(stub).delegate = myStubImpl.
  function bindHandleToStub(handle, localInterface) {
    if (!core.isHandle(handle))
      throw new Error("Not a handle " + handle);

    var stub = new localInterface.stubClass;
    var router = new Router(handle);
    var connection = new BaseConnection(stub, undefined, router);
    StubBindings(stub).connection = connection;
    return stub;
  }

  /**
   * Creates a messape pipe and links one end of the pipe to the given object.
   * @param {!Object} obj The object to create a handle for. Must be a subclass
   *     of an auto-generated stub class.
   * @return {!MojoHandle} The other (not yet connected) end of the message
   *     pipe.
   */
  function bindStubDerivedImpl(obj) {
    var pipe = core.createMessagePipe();
    var router = new Router(pipe.handle0);
    var connection = new BaseConnection(obj, undefined, router);
    obj.connection = connection;
    return pipe.handle1;
  }

  var exports = {};
  exports.Connection = Connection;
  exports.EmptyProxy = ProxyBase;
  exports.EmptyStub = StubBase;
  exports.ProxyBase = ProxyBase;
  exports.ProxyBindings = ProxyBindings;
  exports.StubBase = StubBase;
  exports.StubBindings = StubBindings;
  exports.TestConnection = TestConnection;

  exports.bindProxy = bindProxy;
  exports.getProxy = getProxy;
  exports.bindImpl = bindImpl;
  exports.bindHandleToProxy = bindHandleToProxy;
  exports.bindHandleToStub = bindHandleToStub;
  exports.bindStubDerivedImpl = bindStubDerivedImpl;
  return exports;
});
