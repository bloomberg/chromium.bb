// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/bindings", [
  "mojo/public/js/connection",
  "mojo/public/js/core",
  "mojo/public/js/interface_types",
], function(connection, core, types) {

  // ---------------------------------------------------------------------------

  function makeRequest(interfacePtr) {
    var pipe = core.createMessagePipe();
    interfacePtr.ptr.bind(new types.InterfacePtrInfo(pipe.handle0, 0));
    return new types.InterfaceRequest(pipe.handle1);
  }

  // ---------------------------------------------------------------------------

  // Operations used to setup/configure an interface pointer. Exposed as the
  // |ptr| field of generated interface pointer classes.
  // |ptrInfoOrHandle| could be omitted and passed into bind() later.
  function InterfacePtrController(interfaceType, ptrInfoOrHandle) {
    this.version = 0;

    this.interfaceType_ = interfaceType;
    this.connection_ = null;
    // |connection_| is lazily initialized. |handle_| is valid between bind()
    // and the initialization of |connection_|.
    this.handle_ = null;

    if (ptrInfoOrHandle)
      this.bind(ptrInfoOrHandle);
  }

  InterfacePtrController.prototype.bind = function(ptrInfoOrHandle) {
    this.reset();

    if (ptrInfoOrHandle instanceof types.InterfacePtrInfo) {
      this.version = ptrInfoOrHandle.version;
      this.handle_ = ptrInfoOrHandle.handle;
    } else {
      this.handle_ = ptrInfoOrHandle;
    }
  };

  InterfacePtrController.prototype.isBound = function() {
    return this.connection_ !== null || this.handle_ !== null;
  };

  // Although users could just discard the object, reset() closes the pipe
  // immediately.
  InterfacePtrController.prototype.reset = function() {
    this.version = 0;
    if (this.connection_) {
      this.connection_.close();
      this.connection_ = null;
    }
    if (this.handle_) {
      core.close(this.handle_);
      this.handle_ = null;
    }
  };

  InterfacePtrController.prototype.setConnectionErrorHandler
      = function(callback) {
    if (!this.isBound())
      throw new Error("Cannot set connection error handler if not bound.");

    this.configureProxyIfNecessary_();
    this.connection_.router_.setErrorHandler(callback);
  };

  InterfacePtrController.prototype.passInterface = function() {
    var result;
    if (this.connection_) {
      result = new types.InterfacePtrInfo(
          this.connection_.router_.connector_.handle_, this.version);
      this.connection_.router_.connector_.handle_ = null;
    } else {
      // This also handles the case when this object is not bound.
      result = new types.InterfacePtrInfo(this.handle_, this.version);
      this.handle_ = null;
    }

    this.reset();
    return result;
  };

  InterfacePtrController.prototype.getProxy = function() {
    this.configureProxyIfNecessary_();
    return this.connection_.remote;
  };

  InterfacePtrController.prototype.configureProxyIfNecessary_ = function() {
    if (!this.handle_)
      return;

    this.connection_ = new connection.Connection(
        this.handle_, undefined, this.interfaceType_.proxyClass);
    this.handle_ = null;
  };

  // TODO(yzshen): Implement the following methods.
  //   InterfacePtrController.prototype.queryVersion
  //   InterfacePtrController.prototype.requireVersion

  // ---------------------------------------------------------------------------

  // |request| could be omitted and passed into bind() later.
  //
  // Example:
  //
  //    // FooImpl implements mojom.Foo.
  //    function FooImpl() { ... }
  //    FooImpl.prototype.fooMethod1 = function() { ... }
  //    FooImpl.prototype.fooMethod2 = function() { ... }
  //
  //    var fooPtr = new mojom.FooPtr();
  //    var request = makeRequest(fooPtr);
  //    var binding = new Binding(mojom.Foo, new FooImpl(), request);
  //    fooPtr.fooMethod1();
  function Binding(interfaceType, impl, requestOrHandle) {
    this.interfaceType_ = interfaceType;
    this.impl_ = impl;
    this.stub_ = null;

    if (requestOrHandle)
      this.bind(requestOrHandle);
  }

  Binding.prototype.isBound = function() {
    return this.stub_ !== null;
  };

  Binding.prototype.createInterfacePtrAndBind = function() {
    var ptr = new this.interfaceType_.ptrClass();
    // TODO(yzshen): Set the version of the interface pointer.
    this.bind(makeRequest(ptr));
    return ptr;
  }

  Binding.prototype.bind = function(requestOrHandle) {
    this.close();

    var handle = requestOrHandle instanceof types.InterfaceRequest ?
        requestOrHandle.handle : requestOrHandle;
    if (core.isHandle(handle)) {
      this.stub_ = connection.bindHandleToStub(handle, this.interfaceType_);
      connection.StubBindings(this.stub_).delegate = this.impl_;
    }
  };

  Binding.prototype.close = function() {
    if (!this.isBound())
      return;
    connection.StubBindings(this.stub_).close();
    this.stub_ = null;
  };

  Binding.prototype.setConnectionErrorHandler
      = function(callback) {
    if (!this.isBound())
      throw new Error("Cannot set connection error handler if not bound.");
    connection.StubBindings(this.stub_).connection.router_.setErrorHandler(
        callback);
  };

  Binding.prototype.unbind = function() {
    if (!this.isBound())
      return new types.InterfaceRequest(null);

    var result = new types.InterfaceRequest(
        connection.StubBindings(this.stub_).connection.router_.connector_
            .handle_);
    connection.StubBindings(this.stub_).connection.router_.connector_.handle_ =
        null;
    this.close();
    return result;
  };

  // ---------------------------------------------------------------------------

  function BindingSetEntry(bindingSet, interfaceType, impl, requestOrHandle,
                           bindingId) {
    this.bindingSet_ = bindingSet;
    this.bindingId_ = bindingId;
    this.binding_ = new Binding(interfaceType, impl, requestOrHandle);

    this.binding_.setConnectionErrorHandler(
        () => this.bindingSet_.onConnectionError(bindingId));
  }

  BindingSetEntry.prototype.close = function() {
    this.binding_.close();
  };

  function BindingSet(interfaceType) {
    this.interfaceType_ = interfaceType;
    this.nextBindingId_ = 0;
    this.bindings_ = new Map();
    this.errorHandler_ = null;
  }

  BindingSet.prototype.isEmpty = function() {
    return this.bindings_.size == 0;
  };

  BindingSet.prototype.addBinding = function(impl, requestOrHandle) {
    this.bindings_.set(
        this.nextBindingId_,
        new BindingSetEntry(this, this.interfaceType_, impl, requestOrHandle,
                            this.nextBindingId_));
    ++this.nextBindingId_;
  };

  BindingSet.prototype.closeAllBindings = function() {
    for (var entry of this.bindings_.values())
      entry.close();
    this.bindings_.clear();
  };

  BindingSet.prototype.setConnectionErrorHandler = function(callback) {
    this.errorHandler_ = callback;
  };

  BindingSet.prototype.onConnectionError = function(bindingId) {
    this.bindings_.delete(bindingId);

    if (this.errorHandler_)
      this.errorHandler_();
  };

  var exports = {};
  exports.InterfacePtrInfo = types.InterfacePtrInfo;
  exports.InterfaceRequest = types.InterfaceRequest;
  exports.makeRequest = makeRequest;
  exports.InterfacePtrController = InterfacePtrController;
  exports.Binding = Binding;
  exports.BindingSet = BindingSet;

  // TODO(yzshen): Remove the following exports.
  exports.EmptyProxy = connection.EmptyProxy;
  exports.EmptyStub = connection.EmptyStub;
  exports.ProxyBase = connection.ProxyBase;
  exports.ProxyBindings = connection.ProxyBindings;
  exports.StubBase = connection.StubBase;
  exports.StubBindings = connection.StubBindings;

  return exports;
});
