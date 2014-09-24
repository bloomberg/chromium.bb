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
    // TODO(hansmuller): nr.name should be null, see crbug.com/417039.
    expect(nr.name).toBe("");
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

  testConstructors();
  this.result = "PASS";
});