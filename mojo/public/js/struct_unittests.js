// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "gin/test/expect",
    "mojo/public/interfaces/bindings/tests/rect.mojom",
    "mojo/public/interfaces/bindings/tests/test_structs.mojom",
    "mojo/public/js/codec",
    "mojo/public/js/validator",
], function(expect,
            rect,
            testStructs,
            codec,
            validator) {

  function testConstructors() {
    var r = new rect.Rect();
    expect(r).toEqual(new rect.Rect({x:0, y:0, width:0, height:0}));
    expect(r).toEqual(new rect.Rect({foo:100, bar:200}));

    r.x = 10;
    r.y = 20;
    r.width = 30;
    r.height = 40;
    var rp = new testStructs.RectPair({first: r, second: r});
    expect(rp.first).toEqual(r);
    expect(rp.second).toEqual(r);

    expect(new testStructs.RectPair({second: r}).first).toBeNull();

    var nr = new testStructs.NamedRegion();
    expect(nr.name).toBeNull();
    expect(nr.rects).toBeNull();
    expect(nr).toEqual(new testStructs.NamedRegion({}));

    nr.name = "foo";
    nr.rects = [r, r, r];
    expect(nr).toEqual(new testStructs.NamedRegion({
      name: "foo",
      rects: [r, r, r],
    }));

    var e = new testStructs.EmptyStruct();
    expect(e).toEqual(new testStructs.EmptyStruct({foo:123}));
  }

  function testNoDefaultFieldValues() {
    var s = new testStructs.NoDefaultFieldValues();
    expect(s.f0).toEqual(false);

    // f1 - f10, number type fields
    for (var i = 1; i <= 10; i++)
      expect(s["f" + i]).toEqual(0);

    // f11,12 strings, f13-22 handles, f23-f26 arrays, f27,28 structs
    for (var i = 11; i <= 28; i++)
      expect(s["f" + i]).toBeNull();
  }

  function testDefaultFieldValues() {
    var s = new testStructs.DefaultFieldValues();
    expect(s.f0).toEqual(true);

    // f1 - f12, number type fields
    for (var i = 1; i <= 12; i++)
      expect(s["f" + i]).toEqual(100);

    // f13,14 "foo"
    for (var i = 13; i <= 14; i++)
      expect(s["f" + i]).toEqual("foo");

    // f15,16 a default instance of Rect
    var r = new rect.Rect();
    expect(s.f15).toEqual(r);
    expect(s.f16).toEqual(r);
  }

  function testScopedConstants() {
    expect(testStructs.ScopedConstants.TEN).toEqual(10);
    expect(testStructs.ScopedConstants.ALSO_TEN).toEqual(10);
    expect(testStructs.ScopedConstants.TEN_TOO).toEqual(10);

    expect(testStructs.ScopedConstants.EType.E0).toEqual(0);
    expect(testStructs.ScopedConstants.EType.E1).toEqual(1);
    expect(testStructs.ScopedConstants.EType.E2).toEqual(10);
    expect(testStructs.ScopedConstants.EType.E3).toEqual(10);
    expect(testStructs.ScopedConstants.EType.E4).toEqual(11);

    var s = new testStructs.ScopedConstants();
    expect(s.f0).toEqual(0);
    expect(s.f1).toEqual(1);
    expect(s.f2).toEqual(10);
    expect(s.f3).toEqual(10);
    expect(s.f4).toEqual(11);
    expect(s.f5).toEqual(10);
    expect(s.f6).toEqual(10);
  }

  function structEncodeDecode(struct) {
    var structClass = struct.constructor;
    var builder = new codec.MessageBuilder(1234, structClass.encodedSize);
    builder.encodeStruct(structClass, struct);
    var message = builder.finish();

    var messageValidator = new validator.Validator(message);
    var err = structClass.validate(messageValidator, codec.kMessageHeaderSize);
    expect(err).toEqual(validator.validationError.NONE);

    var reader = new codec.MessageReader(message);
    return reader.decodeStruct(structClass);
  }

  function testMapKeyTypes() {
    var mapFieldsStruct = new testStructs.MapKeyTypes({
      f0: new Map([[true, false], [false, true]]),  // map<bool, bool>
      f1: new Map([[0, 0], [1, 127], [-1, -128]]),  // map<int8, int8>
      f2: new Map([[0, 0], [1, 127], [2, 255]]),  // map<uint8, uint8>
      f3: new Map([[0, 0], [1, 32767], [2, -32768]]),  // map<int16, int16>
      f4: new Map([[0, 0], [1, 32768], [2, 0xFFFF]]),  // map<uint16, uint16>
      f5: new Map([[0, 0], [1, 32767], [2, -32768]]),  // map<int32, int32>
      f6: new Map([[0, 0], [1, 32768], [2, 0xFFFF]]),  // map<uint32, uint32>
      f7: new Map([[0, 0], [1, 32767], [2, -32768]]),  // map<int64, int64>
      f8: new Map([[0, 0], [1, 32768], [2, 0xFFFF]]),  // map<uint64, uint64>
      f9: new Map([[1000.5, -50000], [100.5, 5000]]),  // map<float, float>
      f10: new Map([[-100.5, -50000], [0, 50000000]]),  // map<double, double>
      f11: new Map([["one", "two"], ["free", "four"]]),  // map<string, string>
    });
    var decodedStruct = structEncodeDecode(mapFieldsStruct);
    expect(decodedStruct.f0).toEqual(mapFieldsStruct.f0);
    expect(decodedStruct.f1).toEqual(mapFieldsStruct.f1);
    expect(decodedStruct.f2).toEqual(mapFieldsStruct.f2);
    expect(decodedStruct.f3).toEqual(mapFieldsStruct.f3);
    expect(decodedStruct.f4).toEqual(mapFieldsStruct.f4);
    expect(decodedStruct.f5).toEqual(mapFieldsStruct.f5);
    expect(decodedStruct.f6).toEqual(mapFieldsStruct.f6);
    expect(decodedStruct.f7).toEqual(mapFieldsStruct.f7);
    expect(decodedStruct.f8).toEqual(mapFieldsStruct.f8);
    expect(decodedStruct.f9).toEqual(mapFieldsStruct.f9);
    expect(decodedStruct.f10).toEqual(mapFieldsStruct.f10);
    expect(decodedStruct.f11).toEqual(mapFieldsStruct.f11);
  }

  function testMapValueTypes() {
    var mapFieldsStruct = new testStructs.MapValueTypes({
      f0: new Map([["a", ["b", "c"]], ["d", ["e"]]]),  // array<string>>
      f1: new Map([["a", null], ["b", ["c", "d"]]]),  // array<string>?>
      f2: new Map([["a", [null]], ["b", [null, "d"]]]),  // array<string?>>
      f3: new Map([["a", ["1", "2"]], ["b", ["1", "2"]]]),  // array<string,2>>
      f4: new Map([["a", [["1"]]], ["b", [null]]]),  // array<array<string, 1>?>
      f5: new Map([["a", [["1", "2"]]]]),  // array<array<string, 2>, 1>>
    });
    var decodedStruct = structEncodeDecode(mapFieldsStruct);
    expect(decodedStruct.f0).toEqual(mapFieldsStruct.f0);
    expect(decodedStruct.f1).toEqual(mapFieldsStruct.f1);
    expect(decodedStruct.f2).toEqual(mapFieldsStruct.f2);
    expect(decodedStruct.f3).toEqual(mapFieldsStruct.f3);
    expect(decodedStruct.f4).toEqual(mapFieldsStruct.f4);
    expect(decodedStruct.f5).toEqual(mapFieldsStruct.f5);
  }

  testConstructors();
  testNoDefaultFieldValues();
  testDefaultFieldValues();
  testScopedConstants();
  testMapKeyTypes();
  testMapValueTypes();
  this.result = "PASS";
});
