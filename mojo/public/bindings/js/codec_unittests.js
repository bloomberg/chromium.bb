// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "gtest",
    "mojo/public/bindings/js/codec",
    "mojo/public/bindings/js/test/sample_service"
  ], function(gtest, codec, sample) {
  testBar();
  testFoo();
  this.result = "PASS";

  function barMatches(bar, expected) {
    gtest.expectEqual(bar.alpha, expected.alpha,
        "bar.alpha is " + bar.alpha);
    gtest.expectEqual(bar.beta, expected.beta,
        "bar.beta is " + bar.beta);
    gtest.expectEqual(bar.gamma, expected.gamma,
        "bar.gamma is " + bar.gamma);
  }

  function equalsArray(actual, expected, tag) {
    for (var i = 0; i < expected.length; ++i) {
      gtest.expectEqual(actual[i], expected[i],
          tag + "[" + i + "] is " + actual[i]);
    }
  }

  function testBar() {
    var bar = new sample.Bar();
    bar.alpha = 1;
    bar.beta = 2;
    bar.gamma = 3;
    bar.extraProperty = "banana";

    var messageName = 42;
    var payloadSize = sample.Bar.encodedSize;

    var builder = new codec.MessageBuilder(messageName, payloadSize);
    builder.encodeStruct(sample.Bar, bar);

    var message = builder.finish();

    var expectedMemory = [
      24, 0, 0, 0,
      42, 0, 0, 0,

      16, 0, 0, 0,
       3, 0, 0, 0,

       1, 2, 3, 0,
       0, 0, 0, 0,
    ];

    for (var i = 0; i < expectedMemory.length; ++i) {
      gtest.expectEqual(message.memory[i], expectedMemory[i],
          "message.memory[" + i + "] is " + message.memory[i]);
    }

    var reader = new codec.MessageReader(message);

    gtest.expectEqual(reader.payloadSize, payloadSize,
        "reader.payloadSize is " + reader.payloadSize);
    gtest.expectEqual(reader.messageName, messageName,
        "reader.messageName is " + reader.messageName);

    var bar2 = reader.decodeStruct(sample.Bar);

    barMatches(bar2, bar);
    gtest.expectFalse("extraProperty" in bar2,
        "extraProperty appeared in bar2")
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
    foo.files = [
      // These are supposed to be handles, but we fake them with integers.
      23423782, 32549823, 98320423, 38502383, 92834093,
    ];

    var messageName = 31;
    var payloadSize = sample.Foo.encodedSize;

    var builder = new codec.MessageBuilder(messageName, payloadSize);
    builder.encodeStruct(sample.Foo, foo);

    var message = builder.finish();

    var expectedMemory = [
      /*  0: */   80,    0,    0,    0,   31,    0,    0,    0,
      /*  8: */   72,    0,    0,    0,   10,    0,    0,    0,
      /* 16: */ 0xD5, 0xB4, 0x12, 0x02, 0x93, 0x6E, 0x01,    0,
      /* 24: */    5,    0,    0,    0,    0,    0,    0,    0,
      /* 32: */   48,    0,    0,    0,    0,    0,    0,    0,
      // TODO(abarth): Test more of the message's raw memory.
    ];

    equalsArray(message.memory, expectedMemory, "message.memory");

    var expectedHandles = [
      23423782, 32549823, 98320423, 38502383, 92834093,
    ];

    equalsArray(message.handles, expectedHandles, "message.handles");

    var reader = new codec.MessageReader(message);

    gtest.expectEqual(reader.payloadSize, payloadSize,
        "reader.payloadSize is " + reader.payloadSize);
    gtest.expectEqual(reader.messageName, messageName,
        "reader.messageName is " + reader.messageName);

    var foo2 = reader.decodeStruct(sample.Foo);

    gtest.expectEqual

    gtest.expectEqual(foo2.x, foo.x,
        "foo2.x is " + foo2.x);
    gtest.expectEqual(foo2.y, foo.y,
        "foo2.y is " + foo2.y);

    gtest.expectEqual(foo2.a, foo.a & 1,
        "foo2.a is " + foo2.a);
    gtest.expectEqual(foo2.b, foo.b & 1,
        "foo2.b is " + foo2.b);
    gtest.expectEqual(foo2.c, foo.c & 1,
        "foo2.c is " + foo2.c);

    barMatches(foo2.bar, foo.bar);
    equalsArray(foo2.data, foo.data, "foo2.data");

    var actualExtraBarsJSON = JSON.stringify(foo2.extra_bars);
    var expectedExtraBarsJSON = JSON.stringify(foo.extra_bars);
    gtest.expectEqual(actualExtraBarsJSON, expectedExtraBarsJSON,
        actualExtraBarsJSON);

    gtest.expectEqual(foo2.name, foo.name,
        "foo2.name is " + foo2.name);

    equalsArray(foo2.files, foo.files, "foo2.files");
  }
});
