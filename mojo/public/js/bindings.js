// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/bindings", [
  "mojo/public/js/connection",
  "mojo/public/js/core",
], function(connection, core) {

  // ---------------------------------------------------------------------------

  function InterfacePtrInfo(handle, version) {
    this.handle = handle;
    this.version = version;
  }

  InterfacePtrInfo.prototype.isValid = function() {
    return core.isHandle(this.handle);
  }

  // ---------------------------------------------------------------------------

  function InterfaceRequest(handle) {
    this.handle = handle;
  }

  InterfaceRequest.prototype.isValid = function() {
    return core.isHandle(this.handle);
  }

  // ---------------------------------------------------------------------------

  function makeRequest(interfacePtr) {
    var pipe = core.createMessagePipe();
    interfacePtr.ptr.bind(new InterfacePtrInfo(pipe.handle0, 0));
    return new InterfaceRequest(pipe.handle1);
  }

  // ---------------------------------------------------------------------------

  // Operations used to setup/configure an interface pointer. Exposed as the
  // |ptr| field of generated interface pointer classes.
  function InterfacePtrController(interfaceType) {
    this.version = 0;
    this.connection = null;

    this.interfaceType_ = interfaceType;
  }

  InterfacePtrController.prototype.bind = function(interfacePtrInfo) {
    this.reset();
    this.version = interfacePtrInfo.version;
    this.connection = new connection.Connection(
      interfacePtrInfo.handle, undefined, this.interfaceType_.proxyClass);
  }

  InterfacePtrController.prototype.isBound = function() {
    return this.connection !== null;
  }

  // Although users could just discard the object, reset() closes the pipe
  // immediately.
  InterfacePtrController.prototype.reset = function() {
    if (!this.isBound())
      return;

    this.version = 0;
    this.connection.close();
    this.connection = null;
  }

  // TODO(yzshen): Implement the following methods.
  //   InterfacePtrController.prototype.setConnectionErrorHandler
  //   InterfacePtrController.prototype.passInterface
  //   InterfacePtrController.prototype.queryVersion
  //   InterfacePtrController.prototype.requireVersion

  // ---------------------------------------------------------------------------

  // |request| could be omitted and passed into bind() later.
  // NOTE: |impl| shouldn't hold a reference to this object, because that
  // results in circular references.
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
  function Binding(interfaceType, impl, request) {
    this.interfaceType_ = interfaceType;
    this.impl_ = impl;
    this.stub_ = null;

    if (request)
      this.bind(request);
  }

  Binding.prototype.isBound = function() {
    return this.stub_ !== null;
  }

  Binding.prototype.bind = function(request) {
    this.close();
    this.stub_ = connection.bindHandleToStub(request.handle,
                                             this.interfaceType_);
    connection.StubBindings(this.stub_).delegate = this.impl_;
  }

  Binding.prototype.close = function() {
    if (!this.isBound())
      return;
    connection.StubBindings(this.stub_).close();
    this.stub_ = null;
  }

  // TODO(yzshen): Implement the following methods.
  //   Binding.prototype.setConnectionErrorHandler
  //   Binding.prototype.unbind

  var exports = {};
  exports.InterfacePtrInfo = InterfacePtrInfo;
  exports.InterfaceRequest = InterfaceRequest;
  exports.makeRequest = makeRequest;
  exports.InterfacePtrController = InterfacePtrController;
  exports.Binding = Binding;

  // TODO(yzshen): Remove the following exports.
  exports.EmptyProxy = connection.EmptyProxy;
  exports.EmptyStub = connection.EmptyStub;
  exports.ProxyBase = connection.ProxyBase;
  exports.ProxyBindings = connection.ProxyBindings;
  exports.StubBase = connection.StubBase;
  exports.StubBindings = connection.StubBindings;

  return exports;
});
