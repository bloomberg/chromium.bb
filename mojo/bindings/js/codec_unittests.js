// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "gin/test/expect",
    "mojo/public/js/bindings/codec",
    "mojo/public/interfaces/bindings/tests/sample_service.mojom",
  ], function(expect, codec, sample) {
  testBar();
  testFoo();
  testAlign();
  testUtf8();
  this.result = "PASS";

  function testBar() {
    var bar = new sample.Bar();
    bar.alpha = 1;
    bar.beta = 2;
    bar.gamma = 3;
    bar.type = 0x08070605;
    bar.extraProperty = "banana";

    var messageName = 42;
    var payloadSize = sample.Bar.encodedSize;

    var builder = new codec.MessageBuilder(messageName, payloadSize);
    builder.encodeStruct(sample.Bar, bar);

    var message = builder.finish();

    var expectedMemory = new Uint8Array([
      16, 0, 0, 0,
       2, 0, 0, 0,
      42, 0, 0, 0,
       0, 0, 0, 0,

      16, 0, 0, 0,
       4, 0, 0, 0,

       1, 2, 3, 0,
       5, 6, 7, 8,
    ]);

    expect(message.memory).toEqual(expectedMemory);

    var reader = new codec.MessageReader(message);

    expect(reader.payloadSize).toBe(payloadSize);
    expect(reader.messageName).toBe(messageName);

    var bar2 = reader.decodeStruct(sample.Bar);

    expect(bar2.alpha).toBe(bar.alpha);
    expect(bar2.beta).toBe(bar.beta);
    expect(bar2.gamma).toBe(bar.gamma);
    expect("extraProperty" in bar2).toBeFalsy();
  }

  function testFoo() {
    var foo = new sample.Foo();
    foo.x = 0x212B4D5;
    foo.y = 0x16E93;
    foo.a = 1;
    foo.b = 0;
    foo.c = 3; // This will get truncated to one bit.
    foo.bar = new sample.Bar();
    foo.bar.alpha = 91;
    foo.bar.beta = 82;
    foo.bar.gamma = 73;
    foo.data = [
      4, 5, 6, 7, 8,
    ];
    foo.extra_bars = [
      new sample.Bar(), new sample.Bar(), new sample.Bar(),
    ];
    for (var i = 0; i < foo.extra_bars.length; ++i) {
      foo.extra_bars[i].alpha = 1 * i;
      foo.extra_bars[i].beta = 2 * i;
      foo.extra_bars[i].gamma = 3 * i;
    }
    foo.name = "I am a banana";
    // This is supposed to be a handle, but we fake it with an integer.
    foo.source = 23423782;

    var messageName = 31;
    var payloadSize = 224;

    var builder = new codec.MessageBuilder(messageName, payloadSize);
    builder.encodeStruct(sample.Foo, foo);

    var message = builder.finish();

    var expectedMemory = new Uint8Array([
      /*  0: */   16,    0,    0,    0,    2,    0,    0,    0,
      /*  8: */   31,    0,    0,    0,    0,    0,    0,    0,
      /* 16: */   72,    0,    0,    0,   12,    0,    0,    0,
      /* 24: */ 0xD5, 0xB4, 0x12, 0x02, 0x93, 0x6E, 0x01,    0,
      /* 32: */    5,    0,    0,    0,    0,    0,    0,    0,
      /* 40: */   48,    0,    0,    0,    0,    0,    0,    0,
    ]);
    // TODO(abarth): Test more of the message's raw memory.
    var actualMemory = new Uint8Array(message.memory.buffer,
                                      0, expectedMemory.length);
    expect(actualMemory).toEqual(expectedMemory);

    var expectedHandles = [
      23423782,
    ];

    expect(message.handles).toEqual(expectedHandles);

    var reader = new codec.MessageReader(message);

    expect(reader.payloadSize).toBe(payloadSize);
    expect(reader.messageName).toBe(messageName);

    var foo2 = reader.decodeStruct(sample.Foo);

    expect(foo2.x).toBe(foo.x);
    expect(foo2.y).toBe(foo.y);

    expect(foo2.a).toBe(foo.a & 1 ? true : false);
    expect(foo2.b).toBe(foo.b & 1 ? true : false);
    expect(foo2.c).toBe(foo.c & 1 ? true : false);

    expect(foo2.bar).toEqual(foo.bar);
    expect(foo2.data).toEqual(foo.data);

    expect(foo2.extra_bars).toEqual(foo.extra_bars);
    expect(foo2.name).toBe(foo.name);
    expect(foo2.source).toEqual(foo.source);
  }

  function testAlign() {
    var aligned = [
      0, // 0
      8, // 1
      8, // 2
      8, // 3
      8, // 4
      8, // 5
      8, // 6
      8, // 7
      8, // 8
      16, // 9
      16, // 10
      16, // 11
      16, // 12
      16, // 13
      16, // 14
      16, // 15
      16, // 16
      24, // 17
      24, // 18
      24, // 19
      24, // 20
    ];
    for (var i = 0; i < aligned.length; ++i)
      expect(codec.align(i)).toBe(aligned[i]);
  }

  function testUtf8() {
    var str = "B\u03ba\u1f79";  // some UCS-2 codepoints
    var messageName = 42;
    var payloadSize = 24;

    var builder = new codec.MessageBuilder(messageName, payloadSize);
    var encoder = builder.createEncoder(8);
    encoder.encodeStringPointer(str);
    var message = builder.finish();
    var expectedMemory = new Uint8Array([
      /*  0: */   16,    0,    0,    0,    2,    0,    0,    0,
      /*  8: */   42,    0,    0,    0,    0,    0,    0,    0,
      /* 16: */    8,    0,    0,    0,    0,    0,    0,    0,
      /* 24: */   14,    0,    0,    0,    6,    0,    0,    0,
      /* 32: */ 0x42, 0xCE, 0xBA, 0xE1, 0xBD, 0xB9,    0,    0,
    ]);
    var actualMemory = new Uint8Array(message.memory.buffer);
    expect(actualMemory.length).toEqual(expectedMemory.length);
    expect(actualMemory).toEqual(expectedMemory);

    var reader = new codec.MessageReader(message);
    expect(reader.payloadSize).toBe(payloadSize);
    expect(reader.messageName).toBe(messageName);
    var str2 = reader.decoder.decodeStringPointer();
    expect(str2).toEqual(str);
  }
});
