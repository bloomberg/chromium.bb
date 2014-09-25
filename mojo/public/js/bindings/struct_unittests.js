// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "gin/test/expect",
    "mojo/public/interfaces/bindings/tests/rect.mojom",
    "mojo/public/interfaces/bindings/tests/test_structs.mojom"
], function(expect,
            rect,
            testStructs) {

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

  testConstructors();
  testNoDefaultFieldValues();
  testDefaultFieldValues();
  this.result = "PASS";
});