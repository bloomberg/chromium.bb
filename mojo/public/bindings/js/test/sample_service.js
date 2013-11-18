// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// "Generated" code for sample_service.mojom
define([
    "mojo/public/bindings/js/codec",
  ], function(codec) {

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
    var numberofFields = decoder.read32();
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
  }

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

  var exports = {};
  exports.Bar = Bar;
  exports.Foo = Foo;
  return exports;
});
