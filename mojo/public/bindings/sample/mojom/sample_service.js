// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// "Generated" code for sample_service.mojom
define([
    "mojo/public/bindings/js/core",
    "mojo/public/bindings/js/codec",
  ], function(core, codec) {

  // Bar ----------------------------------------------------------------------

  function Bar() {
    this.alpha = 0;
    this.beta = 0;
    this.gamma = 0;
  }

  Bar.encodedSize = codec.kStructHeaderSize + 8;

  Bar.decode = function(decoder) {
    var val = new Bar();
    var numberOfBytes = decoder.read32();
    var numberOfFields = decoder.read32();
    // TODO(abarth): We need to support optional fields.
    val.alpha = decoder.read8();
    val.beta = decoder.read8();
    val.gamma = decoder.read8();
    decoder.skip(5);
    return val;
  };

  Bar.encode = function(encoder, val) {
    encoder.write32(Bar.encodedSize);
    encoder.write32(3);
    encoder.write8(val.alpha);
    encoder.write8(val.beta);
    encoder.write8(val.gamma);
    encoder.skip(5);
  };

  // Foo ----------------------------------------------------------------------

  function Foo() {
    this.x = 0;
    this.y = 0;
    this.a = 0;
    this.b = 0;
    this.c = 0;
    this.bar = null;
    this.data = [];
    this.extra_bars = [];
    this.name = "";
    this.files = [];
  }

  Foo.encodedSize = codec.kStructHeaderSize + 64;

  Foo.decode = function(decoder) {
    var val = new Foo();
    var numberOfBytes = decoder.read32();
    var numberOfFields = decoder.read32();
    // TODO(abarth): We need to support optional fields.
    val.x = decoder.read32();
    val.y = decoder.read32();
    var packed = decoder.read8();
    val.a = (packed >> 0) & 1;
    val.b = (packed >> 1) & 1;
    val.c = (packed >> 2) & 1;
    decoder.skip(7);
    val.bar = decoder.decodeStructPointer(Bar);
    val.data = decoder.decodeArrayPointer(codec.Uint8);
    val.extra_bars = decoder.decodeArrayPointer(new codec.PointerTo(Bar));
    val.name = decoder.decodeStringPointer();
    val.files = decoder.decodeArrayPointer(codec.Handle);
    return val;
  };

  Foo.encode = function(encoder, val) {
    encoder.write32(Foo.encodedSize);
    encoder.write32(10);
    encoder.write32(val.x);
    encoder.write32(val.y);
    var packed = (val.a & 1) << 0 |
                 (val.b & 1) << 1 |
                 (val.c & 1) << 2;
    encoder.write8(packed);
    encoder.skip(7);
    encoder.encodeStructPointer(Bar, val.bar);
    encoder.encodeArrayPointer(codec.Uint8, val.data);
    encoder.encodeArrayPointer(new codec.PointerTo(Bar), val.extra_bars);
    encoder.encodeStringPointer(val.name);
    encoder.encodeArrayPointer(codec.Handle, val.files);
  };

  // Service ------------------------------------------------------------------

  var kService_Frobinate_Name = 1;

  function Service_Frobinate_Params() {
    this.foo = null;
    this.baz = false;
    this.port = core.kInvalidHandle;
  }

  Service_Frobinate_Params.encodedSize = codec.kStructHeaderSize + 16;

  Service_Frobinate_Params.decode = function(decoder) {
    var val = new Service_Frobinate_Params();
    var numberOfBytes = decoder.read32();
    var numberOfFields = decoder.read32();
    val.foo = decoder.decodeStructPointer(Foo);
    val.baz = decoder.read8() & 1;
    decoder.skip(3);
    val.port = decoder.decodeHandle();
    return val;
  };

  Service_Frobinate_Params.encode = function(encoder, val) {
    encoder.write32(Service_Frobinate_Params.encodedSize);
    encoder.write32(3);
    encoder.encodeStructPointer(Foo, val.foo);
    encoder.write8(val.baz & 1);
    encoder.skip(3);
    encoder.encodeHandle(val.port);
  };

  function ServiceProxy(receiver) {
    this.receiver_ = receiver;
  }

  ServiceProxy.prototype.frobinate = function(foo, baz, port) {
    var params = new Service_Frobinate_Params();
    params.foo = foo;
    params.baz = baz;
    params.port = port;

    var payloadSize = codec.align(Service_Frobinate_Params.encodedSize);
    var builder = new codec.MessageBuilder(kService_Frobinate_Name,
                                           payloadSize);
    builder.encodeStruct(Service_Frobinate_Params, params);
    var message = builder.finish();
    this.receiver_.accept(message);
  };

  function ServiceStub() {
  }

  ServiceStub.prototype.accept = function(message) {
    var reader = new codec.MessageReader(message);
    switch (reader.messageName) {
    case kService_Frobinate_Name:
      var params = reader.decodeStruct(Service_Frobinate_Params);
      this.frobinate(params.foo, params.baz, params.port);
      return true;
    default:
      return false;
    }
  };

  // ServiceClient ------------------------------------------------------------

  var kServiceClient_DidFrobinate_Name = 0;

  function ServiceClient_DidFrobinate_Params() {
    this.result = 0;
  }

  ServiceClient_DidFrobinate_Params.encodedSize = codec.kStructHeaderSize + 8;

  ServiceClient_DidFrobinate_Params.decode = function(decoder) {
    var val = new ServiceClient_DidFrobinate_Params();
    var numberOfBytes = decoder.read32();
    var numberOfFields = decoder.read32();
    val.result = decoder.read32();
    decoder.skip(4);
    return val;
  };

  ServiceClient_DidFrobinate_Params.encode = function(encoder, val) {
    encoder.write32(ServiceClient_DidFrobinate_Params.encodedSize);
    encoder.write32(1);
    encoder.write32(val.result);
    encoder.skip(4);
  };

  function ServiceClientProxy(receiver) {
    this.receiver_ = receiver;
  }

  ServiceClientProxy.prototype.didFrobinate = function(result) {
    var params = new ServiceClient_DidFrobinate_Params();
    params.result = result;

    var payloadSize = codec.align(
        ServiceClient_DidFrobinate_Params.encodedSize);
    var builder = new codec.MessageBuilder(kServiceClient_DidFrobinate_Name,
                                           payloadSize);
    builder.encodeStruct(ServiceClient_DidFrobinate_Params, params);
    var message = builder.finish();
    this.receiver_.accept(message);
  };

  function ServiceClientStub() {
  }

  ServiceClientStub.prototype.accept = function(message) {
    var reader = new codec.MessageReader(message);
    switch (reader.messageName) {
    case kServiceClient_DidFrobinate_Name:
      var params = reader.decodeStruct(kServiceClient_DidFrobinate_Name);
      this.didFrobinate(params.result);
      return true;
    default:
      return false;
    }
  };

  // Exports ------------------------------------------------------------------

  var exports = {};
  exports.Bar = Bar;
  exports.Foo = Foo;
  exports.ServiceProxy = ServiceProxy;
  exports.ServiceStub = ServiceStub;
  exports.ServiceClientProxy = ServiceClientProxy;
  exports.ServiceClientStub = ServiceClientStub;
  return exports;
});
