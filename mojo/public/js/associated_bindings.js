// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/associated_bindings", [
  "mojo/public/js/core",
  "mojo/public/js/interface_types",
  "mojo/public/js/lib/interface_endpoint_client",
  "mojo/public/js/lib/interface_endpoint_handle",
], function(core, types, interfaceEndpointClient, interfaceEndpointHandle) {

  var InterfaceEndpointClient = interfaceEndpointClient.InterfaceEndpointClient;

  // ---------------------------------------------------------------------------

  function makeRequest(associatedInterfacePtrInfo) {
    var {handle0, handle1} =
        interfaceEndpointHandle.createPairPendingAssociation();

    associatedInterfacePtrInfo.interfaceEndpointHandle = handle0;
    associatedInterfacePtrInfo.version = 0;

    var request = new types.AssociatedInterfaceRequest(handle1);
    return request;
  }

  // ---------------------------------------------------------------------------

  // Operations used to setup/configure an associated interface pointer.
  // Exposed as |ptr| field of generated associated interface pointer classes.
  // |associatedPtrInfo| could be omitted and passed into bind() later.
  //
  // Example:
  //    // IntegerSenderImpl implements mojom.IntegerSender
  //    function IntegerSenderImpl() { ... }
  //    IntegerSenderImpl.prototype.echo = function() { ... }
  //
  //    // IntegerSenderConnectionImpl implements mojom.IntegerSenderConnection
  //    function IntegerSenderConnectionImpl() {
  //      this.senderBinding_ = null;
  //    }
  //    IntegerSenderConnectionImpl.prototype.getSender = function(
  //        associatedRequest) {
  //      this.senderBinding_ = new AssociatedBinding(mojom.IntegerSender,
  //          new IntegerSenderImpl(),
  //          associatedRequest);
  //    }
  //
  //    var integerSenderConnection = new mojom.IntegerSenderConnectionPtr();
  //    var integerSenderConnectionBinding = new Binding(
  //        mojom.IntegerSenderConnection,
  //        new IntegerSenderConnectionImpl(),
  //        bindings.makeRequest(integerSenderConnection));
  //
  //    // A locally-created associated interface pointer can only be used to
  //    // make calls when the corresponding associated request is sent over
  //    // another interface (either the master interface or another
  //    // associated interface).
  //    var associatedInterfacePtrInfo = new AssociatedInterfacePtrInfo();
  //    var associatedRequest = makeRequest(interfacePtrInfo);
  //
  //    integerSenderConnection.getSender(associatedRequest);
  //
  //    // Create an associated interface and bind the associated handle.
  //    var integerSender = new mojom.AssociatedIntegerSenderPtr();
  //    integerSender.ptr.bind(associatedInterfacePtrInfo);
  //    integerSender.echo();

  function AssociatedInterfacePtrController(interfaceType, associatedPtrInfo) {
    this.version = 0;

    this.interfaceType_ = interfaceType;
    this.interfaceEndpointClient_ = null;
    this.proxy_ = null;

    if (associatedPtrInfo) {
      this.bind(associatedPtrInfo);
    }
  }

  AssociatedInterfacePtrController.prototype.bind = function(
      associatedPtrInfo) {
    this.reset();
    this.version = associatedPtrInfo.version;

    this.interfaceEndpointClient_ = new InterfaceEndpointClient(
        associatedPtrInfo.interfaceEndpointHandle);

    this.interfaceEndpointClient_ .setPayloadValidators([
        this.interfaceType_.validateResponse]);
    this.proxy_ = new this.interfaceType_.proxyClass(
        this.interfaceEndpointClient_);
  };

  AssociatedInterfacePtrController.prototype.isBound = function() {
    return this.interfaceEndpointClient_ !== null;
  };

  AssociatedInterfacePtrController.prototype.reset = function() {
    this.version = 0;
    if (this.interfaceEndpointClient_) {
      this.interfaceEndpointClient_.close();
      this.interfaceEndpointClient_ = null;
    }
    if (this.proxy_) {
      this.proxy_ = null;
    }
  };

  AssociatedInterfacePtrController.prototype.resetWithReason = function(
      reason) {
    if (this.isBound()) {
      this.interfaceEndpointClient_.close(reason);
      this.interfaceEndpointClient_ = null;
    }
    this.reset();
  };

  AssociatedInterfacePtrController.prototype.setConnectionErrorHandler =
      function(callback) {
    if (!this.isBound()) {
      throw new Error("Cannot set connection error handler if not bound.");
    }

    this.interfaceEndpointClient_.setConnectionErrorHandler(callback);
  };

  AssociatedInterfacePtrController.prototype.passInterface = function() {
    if (!this.isBound()) {
      return new types.AssociatedInterfacePtrInfo(null);
    }

    var result = new types.AssociatedInterfacePtrInfo(
        this.interfaceEndpointClient_.passHandle(), this.version);
    this.reset();
    return result;
  };

  AssociatedInterfacePtrController.prototype.getProxy = function() {
    return this.proxy_;
  };

  AssociatedInterfacePtrController.prototype.queryVersion = function() {
    function onQueryVersion(version) {
      this.version = version;
      return version;
    }

    return this.interfaceEndpointClient_.queryVersion().then(
      onQueryVersion.bind(this));
  };

  AssociatedInterfacePtrController.prototype.requireVersion = function(
      version) {
    if (this.version >= version) {
      return;
    }
    this.version = version;
    this.interfaceEndpointClient_.requireVersion(version);
  };

  // ---------------------------------------------------------------------------

  // |associatedInterfaceRequest| could be omitted and passed into bind()
  // later.
  function AssociatedBinding(interfaceType, impl, associatedInterfaceRequest) {
    this.interfaceType_ = interfaceType;
    this.impl_ = impl;
    this.interfaceEndpointClient_ = null;
    this.stub_ = null;

    if (associatedInterfaceRequest) {
      this.bind(associatedInterfaceRequest);
    }
  }

  AssociatedBinding.prototype.isBound = function() {
    return this.interfaceEndpointClient_ !== null;
  };

  AssociatedBinding.prototype.bind = function(associatedInterfaceRequest) {
    this.close();

    this.stub_ = new this.interfaceType_.stubClass(this.impl_);
    this.interfaceEndpointClient_ = new InterfaceEndpointClient(
        associatedInterfaceRequest.interfaceEndpointHandle, this.stub_,
        this.interfaceType_.kVersion);

    this.interfaceEndpointClient_ .setPayloadValidators([
        this.interfaceType_.validateRequest]);
  };


  AssociatedBinding.prototype.close = function() {
    if (!this.isBound()) {
      return;
    }

    if (this.interfaceEndpointClient_) {
      this.interfaceEndpointClient_.close();
      this.interfaceEndpointClient_ = null;
    }

    this.stub_ = null;
  };

  AssociatedBinding.prototype.closeWithReason = function(reason) {
    if (this.interfaceEndpointClient_) {
      this.interfaceEndpointClient_.close(reason);
      this.interfaceEndpointClient_ = null;
    }
    this.close();
  };

  AssociatedBinding.prototype.setConnectionErrorHandler = function(callback) {
    if (!this.isBound()) {
      throw new Error("Cannot set connection error handler if not bound.");
    }
    this.interfaceEndpointClient_.setConnectionErrorHandler(callback);
  };

  AssociatedBinding.prototype.unbind = function() {
    if (!this.isBound()) {
      return new types.AssociatedInterfaceRequest(null);
    }

    var result = new types.AssociatedInterfaceRequest(
        this.interfaceEndpointClient_.passHandle());
    this.close();
    return result;
  };

  var exports = {};
  exports.AssociatedInterfacePtrInfo = types.AssociatedInterfacePtrInfo;
  exports.AssociatedInterfaceRequest = types.AssociatedInterfaceRequest;
  exports.makeRequest = makeRequest;
  exports.AssociatedInterfacePtrController = AssociatedInterfacePtrController;
  exports.AssociatedBinding = AssociatedBinding;

  return exports;
});
