// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/bindings", [
  "mojo/public/js/core",
  "mojo/public/js/lib/control_message_proxy",
  "mojo/public/js/interface_types",
  "mojo/public/js/router",
], function(core, controlMessageProxy, types, router) {

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
    this.router_ = null;
    this.proxy_ = null;

    // |router_| is lazily initialized. |handle_| is valid between bind() and
    // the initialization of |router_|.
    this.handle_ = null;
    this.controlMessageProxy_ = null;

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
    return this.router_ !== null || this.handle_ !== null;
  };

  // Although users could just discard the object, reset() closes the pipe
  // immediately.
  InterfacePtrController.prototype.reset = function() {
    this.version = 0;
    if (this.router_) {
      this.router_.close();
      this.router_ = null;

      this.proxy_ = null;
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
    this.router_.setErrorHandler(callback);
  };

  InterfacePtrController.prototype.passInterface = function() {
    var result;
    if (this.router_) {
      // TODO(yzshen): Fix Router interface to support extracting handle.
      result = new types.InterfacePtrInfo(
          this.router_.connector_.handle_, this.version);
      this.router_.connector_.handle_ = null;
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
    return this.proxy_;
  };

  InterfacePtrController.prototype.enableTestingMode = function() {
    this.configureProxyIfNecessary_();
    return this.router_.enableTestingMode();
  };

  InterfacePtrController.prototype.configureProxyIfNecessary_ = function() {
    if (!this.handle_)
      return;

    this.router_ = new router.Router(this.handle_);
    this.handle_ = null;
    this.router_ .setPayloadValidators([this.interfaceType_.validateResponse]);

    this.controlMessageProxy_ = new
        controlMessageProxy.ControlMessageProxy(this.router_);

    this.proxy_ = new this.interfaceType_.proxyClass(this.router_);
  };

  InterfacePtrController.prototype.queryVersion = function() {
    function onQueryVersion(version) {
      this.version = version;
      return version;
    }

    this.configureProxyIfNecessary_();
    return this.controlMessageProxy_.queryVersion().then(
      onQueryVersion.bind(this));
  };

  InterfacePtrController.prototype.requireVersion = function(version) {
    this.configureProxyIfNecessary_();

    if (this.version >= version) {
      return;
    }
    this.version = version;
    this.controlMessageProxy_.requireVersion(version);
  };

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
    this.router_ = null;
    this.stub_ = null;

    if (requestOrHandle)
      this.bind(requestOrHandle);
  }

  Binding.prototype.isBound = function() {
    return this.router_ !== null;
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
    if (!core.isHandle(handle))
      return;

    this.stub_ = new this.interfaceType_.stubClass(this.impl_);
    this.router_ = new router.Router(handle, this.interfaceType_.kVersion);
    this.router_.setIncomingReceiver(this.stub_);
    this.router_ .setPayloadValidators([this.interfaceType_.validateRequest]);
  };

  Binding.prototype.close = function() {
    if (!this.isBound())
      return;

    this.router_.close();
    this.router_ = null;
    this.stub_ = null;
  };

  Binding.prototype.setConnectionErrorHandler
      = function(callback) {
    if (!this.isBound())
      throw new Error("Cannot set connection error handler if not bound.");
    this.router_.setErrorHandler(callback);
  };

  Binding.prototype.unbind = function() {
    if (!this.isBound())
      return new types.InterfaceRequest(null);

    var result = new types.InterfaceRequest(this.router_.connector_.handle_);
    this.router_.connector_.handle_ = null;
    this.close();
    return result;
  };

  Binding.prototype.enableTestingMode = function() {
    return this.router_.enableTestingMode();
  };

  // ---------------------------------------------------------------------------

  function BindingSetEntry(bindingSet, interfaceType, impl, requestOrHandle,
                           bindingId) {
    this.bindingSet_ = bindingSet;
    this.bindingId_ = bindingId;
    this.binding_ = new Binding(interfaceType, impl, requestOrHandle);

    this.binding_.setConnectionErrorHandler(function() {
      this.bindingSet_.onConnectionError(bindingId);
    }.bind(this));
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

  return exports;
});
