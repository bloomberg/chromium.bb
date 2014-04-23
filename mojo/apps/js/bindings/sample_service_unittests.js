// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "console",
    "mojo/apps/js/test/hexdump",
    "gin/test/expect",
    "mojo/public/interfaces/bindings/tests/sample_service.mojom",
    "mojo/public/interfaces/bindings/tests/sample_import.mojom",
    "mojo/public/interfaces/bindings/tests/sample_import2.mojom",
  ], function(console, hexdump, expect, sample, imported, imported2) {

  var global = this;

  // Set this variable to true to print the binary message in hex.
  var dumpMessageAsHex = false;

  function makeFoo() {
    var bar = new sample.Bar();
    bar.alpha = 20;
    bar.beta = 40;
    bar.gamma = 60;
    bar.type = sample.Bar.Type.TYPE_VERTICAL;

    var extra_bars = new Array(3);
    for (var i = 0; i < extra_bars.length; ++i) {
      var base = i * 100;
      var type = i % 2 ?
          sample.Bar.Type.TYPE_VERTICAL : sample.Bar.Type.TYPE_HORIZONTAL;
      extra_bars[i] = new sample.Bar();
      extra_bars[i].alpha = base;
      extra_bars[i].beta = base + 20;
      extra_bars[i].gamma = base + 40;
      extra_bars[i].type = type;
    }

    var data = new Array(10);
    for (var i = 0; i < data.length; ++i) {
      data[i] = data.length - i;
    }

    var source = 0xFFFF;  // Invent a dummy handle.

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
    foo.source = source;
    return foo;
  }

  // Check that the given |Foo| is identical to the one made by |MakeFoo()|.
  function checkFoo(foo) {
    expect(foo.name).toBe("foopy");
    expect(foo.x).toBe(1);
    expect(foo.y).toBe(2);
    expect(foo.a).toBeFalsy();
    expect(foo.b).toBeTruthy();
    expect(foo.c).toBeFalsy();
    expect(foo.bar.alpha).toBe(20);
    expect(foo.bar.beta).toBe(40);
    expect(foo.bar.gamma).toBe(60);
    expect(foo.bar.type).toBe(sample.Bar.Type.TYPE_VERTICAL);

    expect(foo.extra_bars.length).toBe(3);
    for (var i = 0; i < foo.extra_bars.length; ++i) {
      var base = i * 100;
      var type = i % 2 ?
          sample.Bar.Type.TYPE_VERTICAL : sample.Bar.Type.TYPE_HORIZONTAL;
      expect(foo.extra_bars[i].alpha).toBe(base);
      expect(foo.extra_bars[i].beta).toBe(base + 20);
      expect(foo.extra_bars[i].gamma).toBe(base + 40);
      expect(foo.extra_bars[i].type).toBe(type);
    }

    expect(foo.data.length).toBe(10);
    for (var i = 0; i < foo.data.length; ++i)
      expect(foo.data[i]).toBe(foo.data.length - i);

    expect(foo.source).toBe(0xFFFF);
  }

  // Check that values are set to the defaults if we don't override them.
  function checkDefaultValues() {
    var bar = new sample.Bar();
    expect(bar.alpha).toBe(255);

    var foo = new sample.Foo();
    expect(foo.name).toBe("Fooby");
    expect(foo.a).toBeTruthy();

    expect(foo.data.length).toBe(3);
    expect(foo.data[0]).toBe(1);
    expect(foo.data[1]).toBe(2);
    expect(foo.data[2]).toBe(3);

    var inner = new sample.DefaultsTestInner();
    expect(inner.names.length).toBe(1);
    expect(inner.names[0]).toBe("Jim");
    expect(inner.height).toBe(6*12);

    var full = new sample.DefaultsTest();
    expect(full.people.length).toBe(1);
    expect(full.people[0].age).toBe(32);
    expect(full.people[0].names.length).toBe(2);
    expect(full.people[0].names[0]).toBe("Bob");
    expect(full.people[0].names[1]).toBe("Bobby");
    expect(full.people[0].height).toBe(6*12);

    expect(full.point.x).toBe(7);
    expect(full.point.y).toBe(15);

    expect(full.shape_masks.length).toBe(1);
    expect(full.shape_masks[0]).toBe(1 << imported.Shape.SHAPE_RECTANGLE);

    expect(full.thing.shape).toBe(imported.Shape.SHAPE_CIRCLE);
    expect(full.thing.color).toBe(imported2.Color.COLOR_BLACK);
  }

  function ServiceImpl() {
  }

  ServiceImpl.prototype = Object.create(sample.ServiceStub.prototype);

  ServiceImpl.prototype.frobinate = function(foo, baz, port) {
    checkFoo(foo);
    expect(baz).toBe(sample.ServiceStub.BazOptions.BAZ_EXTRA);
    expect(port).toBe(10);
    global.result = "PASS";
  };

  function SimpleMessageReceiver() {
  }

  SimpleMessageReceiver.prototype.accept = function(message) {
    if (dumpMessageAsHex) {
      var uint8Array = new Uint8Array(message.buffer.arrayBuffer);
      console.log(hexdump.dumpArray(uint8Array));
    }
    // Imagine some IPC happened here.
    var serviceImpl = new ServiceImpl();
    serviceImpl.accept(message);
  };

  var receiver = new SimpleMessageReceiver();
  var serviceProxy = new sample.ServiceProxy(receiver);

  checkDefaultValues();

  var foo = makeFoo();
  checkFoo(foo);

  var port = 10;
  serviceProxy.frobinate(foo, sample.ServiceProxy.BazOptions.BAZ_EXTRA, port);
});
