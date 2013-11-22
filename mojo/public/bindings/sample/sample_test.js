// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "console",
    "mojo/public/bindings/js/test/hexdump",
    "gtest",
    // TODO(abarth): We shouldn't need to depend on codec, but there seems to
    // be a bug in the module loading system whereby this test doesn't run if
    // we don't import codec here.
    "mojo/public/bindings/js/codec",
    "mojom/sample_service"
  ], function(console, hexdump, gtest, codec, sample) {

  var global = this;

  // Set this variable to true to print the binary message in hex.
  var dumpMessageAsHex = false;

  function makeFoo() {
    var bar = new sample.Bar();
    bar.alpha = 20;
    bar.beta = 40;
    bar.gamma = 60;

    var extra_bars = new Array(3);
    for (var i = 0; i < extra_bars.length; ++i) {
      var base = i * 100;
      extra_bars[i] = new sample.Bar();
      extra_bars[i].alpha = base;
      extra_bars[i].beta = base + 20;
      extra_bars[i].gamma = base + 40;
    }

    var data = new Array(10);
    for (var i = 0; i < data.length; ++i) {
      data[i] = data.length - i;
    }

    var files = new Array(4);
    for (var i = 0; i < files.length; ++i) {
      files[i] = 0xFFFF - i;
    }

    var foo = new sample.Foo();
    foo.name = "foopy";
    foo.x = 1;
    foo.y = 2;
    foo.a = false;
    foo.b = true;
    foo.c = false;
    foo.bar = bar;
    foo.extra_bars = extra_bars;
    foo.data = data;
    foo.files = files;
    return foo;
  }

  // Check that the given |Foo| is identical to the one made by |MakeFoo()|.
  function checkFoo(foo) {
    gtest.expectEqual(foo.name, "foopy", "foo.name is " + foo.name);
    gtest.expectEqual(foo.x, 1, "foo.x is " + foo.x);
    gtest.expectEqual(foo.y, 2, "foo.y is " + foo.y);
    gtest.expectFalse(foo.a, "foo.a is " + foo.a);
    gtest.expectTrue(foo.b, "foo.b is " + foo.b);
    gtest.expectFalse(foo.c, "foo.c is " + foo.c);
    gtest.expectEqual(foo.bar.alpha, 20, "foo.bar.alpha is " + foo.bar.alpha);
    gtest.expectEqual(foo.bar.beta, 40, "foo.bar.beta is " + foo.bar.beta);
    gtest.expectEqual(foo.bar.gamma, 60, "foo.bar.gamma is " + foo.bar.gamma);

    gtest.expectEqual(foo.extra_bars.length, 3,
        "foo.extra_bars.length is " + foo.extra_bars.length);
    for (var i = 0; i < foo.extra_bars.length; ++i) {
      var base = i * 100;
      gtest.expectEqual(foo.extra_bars[i].alpha, base,
          "foo.extra_bars[" + i + "].alpha is " + foo.extra_bars[i].alpha);
      gtest.expectEqual(foo.extra_bars[i].beta, base + 20,
          "foo.extra_bars[" + i + "].beta is " + foo.extra_bars[i].beta);
      gtest.expectEqual(foo.extra_bars[i].gamma, base + 40,
          "foo.extra_bars[" + i + "].gamma is " + foo.extra_bars[i].gamma);
    }

    gtest.expectEqual(foo.data.length, 10,
        "foo.data.length is " + foo.data.length);
    for (var i = 0; i < foo.data.length; ++i) {
      gtest.expectEqual(foo.data[i], foo.data.length - i,
          "foo.data[" + i + "] is " + foo.data[i]);
    }

    gtest.expectEqual(foo.files.length, 4,
        "foo.files.length " + foo.files.length);
    for (var i = 0; i < foo.files.length; ++i) {
      gtest.expectEqual(foo.files[i], 0xFFFF - i,
          "foo.files[" + i + "] is " + foo.files[i]);
    }
  }

  function ServiceImpl() {
  }

  ServiceImpl.prototype = Object.create(sample.ServiceStub.prototype);

  ServiceImpl.prototype.frobinate = function(foo, baz, port) {
    checkFoo(foo);
    gtest.expectTrue(baz, "baz is " + baz);
    gtest.expectEqual(port, 10, "port is " + port);
    global.result = "PASS";
  };

  function SimpleMessageReceiver() {
  }

  SimpleMessageReceiver.prototype.accept = function(message) {
    if (dumpMessageAsHex)
      console.log(hexdump.dumpArray(message.memory));
    // Imagine some IPC happened here.
    var serviceImpl = new ServiceImpl();
    serviceImpl.accept(message);
  };

  var receiver = new SimpleMessageReceiver();
  var serviceProxy = new sample.ServiceProxy(receiver);

  var foo = makeFoo();
  checkFoo(foo);

  var port = 10;
  serviceProxy.frobinate(foo, true, port);
});
