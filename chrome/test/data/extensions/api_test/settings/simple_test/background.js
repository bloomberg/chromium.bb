// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var succeed = chrome.test.succeed;

function test(stage0) {
  var apis = [
    chrome.storage.sync,
    chrome.storage.local
  ];
  apis.forEach(function(api) {
    api.succeed = chrome.test.callbackPass(api.clear.bind(api));
    stage0.call(api);
  });
}

chrome.test.runTests([
  function getWhenEmpty() {
    function stage0() {
      this.get('foo', stage1.bind(this));
    }
    function stage1(settings) {
      assertEq({}, settings);
      this.get(['foo', 'bar'], stage2.bind(this));
    }
    function stage2(settings) {
      assertEq({}, settings);
      this.get(undefined, stage3.bind(this));
    }
    function stage3(settings) {
      assertEq({}, settings);
      this.succeed();
    }
    test(stage0);
  },

  function getWhenNonempty() {
    function stage0() {
      this.set({
        'foo'  : 'bar',
        'baz'  : 'qux',
        'hello': 'world'
      }, stage1.bind(this));
    }
    function stage1() {
      this.get(['foo', 'baz'], stage2.bind(this));
    }
    function stage2(settings) {
      assertEq({
        'foo': 'bar',
        'baz': 'qux'
      }, settings);
      this.get(['nothing', 'baz', 'hello', 'ignore'], stage3.bind(this));
    }
    function stage3(settings) {
      assertEq({
        'baz'  : 'qux',
        'hello': 'world'
      }, settings);
      this.get(null, stage4.bind(this));
    }
    function stage4(settings) {
      assertEq({
        'foo'  : 'bar',
        'baz'  : 'qux',
        'hello': 'world'
      }, settings);
      this.succeed();
    }
    test(stage0);
  },

  function removeWhenEmpty() {
    function stage0() {
      this.remove('foo', stage1.bind(this));
    }
    function stage1() {
      this.remove(['foo', 'bar'], this.succeed);
    }
    test(stage0);
  },

  function removeWhenNonempty() {
    function stage0() {
      this.set({
        'foo'  : 'bar',
        'baz'  : 'qux',
        'hello': 'world'
      }, stage1.bind(this));
    }
    function stage1() {
      this.remove('foo', stage2.bind(this));
    }
    function stage2() {
      this.get(null, stage3.bind(this));
    }
    function stage3(settings) {
      assertEq({
        'baz'  : 'qux',
        'hello': 'world'
      }, settings);
      this.remove(['baz', 'nothing'], stage4.bind(this));
    }
    function stage4() {
      this.get(null, stage5.bind(this));
    }
    function stage5(settings) {
      assertEq({
        'hello': 'world'
      }, settings);
      this.remove('hello', stage6.bind(this));
    }
    function stage6() {
      this.get(null, stage7.bind(this));
    }
    function stage7(settings) {
      assertEq({}, settings);
      this.succeed();
    }
    test(stage0);
  },

  function setWhenOverwriting() {
    function stage0() {
      this.set({
        'foo'  : 'bar',
        'baz'  : 'qux',
        'hello': 'world'
      }, stage1.bind(this));
    }
    function stage1() {
      this.set({
        'foo'  : 'otherBar',
        'baz'  : 'otherQux'
      }, stage2.bind(this));
    }
    function stage2() {
      this.get(null, stage3.bind(this));
    }
    function stage3(settings) {
      assertEq({
        'foo'  : 'otherBar',
        'baz'  : 'otherQux',
        'hello': 'world'
      }, settings);
      this.set({
        'baz'  : 'anotherQux',
        'hello': 'otherWorld',
        'some' : 'value'
      }, stage4.bind(this));
    }
    function stage4() {
      this.get(null, stage5.bind(this));
    }
    function stage5(settings) {
      assertEq({
        'foo'  : 'otherBar',
        'baz'  : 'anotherQux',
        'hello': 'otherWorld',
        'some' : 'value'
      }, settings);
      this.succeed();
    }
    test(stage0);
  },

  function clearWhenEmpty() {
    function stage0() {
      this.clear(stage1.bind(this));
    }
    function stage1() {
      this.get(null, stage2.bind(this));
    }
    function stage2(settings) {
      assertEq({}, settings);
      this.succeed();
    }
    test(stage0);
  },

  function clearWhenNonempty() {
    function stage0() {
      this.set({
        'foo'  : 'bar',
        'baz'  : 'qux',
        'hello': 'world'
      }, stage1.bind(this));
    }
    function stage1() {
      this.clear(stage2.bind(this));
    }
    function stage2() {
      this.get(null, stage3.bind(this));
    }
    function stage3(settings) {
      assertEq({}, settings);
      this.succeed();
    }
    test(stage0);
  },

  function keysWithDots() {
    function stage0() {
      this.set({
        'foo.bar' : 'baz',
        'one'     : {'two': 'three'}
      }, stage1.bind(this));
    }
    function stage1() {
      this.get(['foo.bar', 'one'], stage2.bind(this));
    }
    function stage2(settings) {
      assertEq({
        'foo.bar' : 'baz',
        'one'     : {'two': 'three'}
      }, settings);
      this.get('one.two', stage3.bind(this));
    }
    function stage3(settings) {
      assertEq({}, settings);
      this.remove(['foo.bar', 'one.two'], stage4.bind(this));
    }
    function stage4() {
      this.get(null, stage5.bind(this));
    }
    function stage5(settings) {
      assertEq({
        'one'     : {'two': 'three'}
      }, settings);
      this.succeed();
    }
    test(stage0);
  },

  function getWithDefaultValues() {
    function stage0() {
      this.get({
        'foo': 'defaultBar',
        'baz': [1, 2, 3]
      }, stage1.bind(this));
    }
    function stage1(settings) {
      assertEq({
        'foo': 'defaultBar',
        'baz': [1, 2, 3]
      }, settings);
      this.get(null, stage2.bind(this));
    }
    function stage2(settings) {
      assertEq({}, settings);
      this.set({'foo': 'bar'}, stage3.bind(this));
    }
    function stage3() {
      this.get({
        'foo': 'defaultBar',
        'baz': [1, 2, 3]
      }, stage4.bind(this));
    }
    function stage4(settings) {
      assertEq({
        'foo': 'bar',
        'baz': [1, 2, 3]
      }, settings);
      this.set({'baz': {}}, stage5.bind(this));
    }
    function stage5() {
      this.get({
        'foo': 'defaultBar',
        'baz': [1, 2, 3]
      }, stage6.bind(this));
    }
    function stage6(settings) {
      assertEq({
        'foo': 'bar',
        'baz': {}
      }, settings);
      this.remove('foo', stage7.bind(this));
    }
    function stage7() {
      this.get({
        'foo': 'defaultBar',
        'baz': [1, 2, 3]
      }, stage8.bind(this));
    }
    function stage8(settings) {
      assertEq({
        'foo': 'defaultBar',
        'baz': {}
      }, settings);
      this.succeed();
    }
    test(stage0);
  },


  function quota() {
    // Just check that the constants are defined; no need to be forced to
    // update them here as well if/when they change.
    assertTrue(chrome.storage.sync.QUOTA_BYTES > 0);
    assertTrue(chrome.storage.sync.QUOTA_BYTES_PER_ITEM > 0);
    assertTrue(chrome.storage.sync.MAX_ITEMS > 0);

    assertTrue(chrome.storage.local.QUOTA_BYTES > 0);
    assertEq('undefined', typeof chrome.storage.local.QUOTA_BYTES_PER_ITEM);
    assertEq('undefined', typeof chrome.storage.local.MAX_ITEMS);

    var area = chrome.storage.sync;
    function stage0() {
      area.getBytesInUse(null, stage1);
    }
    function stage1(bytesInUse) {
      assertEq(0, bytesInUse);
      area.set({ a: 42, b: 43, c: 44 }, stage2);
    }
    function stage2() {
      area.getBytesInUse(null, stage3);
    }
    function stage3(bytesInUse) {
      assertEq(9, bytesInUse);
      area.getBytesInUse('a', stage4);
    }
    function stage4(bytesInUse) {
      assertEq(3, bytesInUse);
      area.getBytesInUse(['a', 'b'], stage5);
    }
    function stage5(bytesInUse) {
      assertEq(6, bytesInUse);
      succeed();
    }
    area.clear(stage0);
  },

  function nullsInArgs() {
    var area = chrome.storage.local;
    function stage0() {
      area.get({
        foo: 'foo',
        bar: null,
        baz: undefined
      }, stage1);
    }
    function stage1(values) {
      assertEq({
        foo: 'foo',
        bar: null,
      }, values);
      area.set({
        foo: 'foo',
        bar: null,
        baz: undefined
      }, area.get.bind(area, stage2));
    }
    function stage2(values) {
      assertEq({
        foo: 'foo',
        bar: null,
      }, values);
      succeed();
    }
    area.clear(stage0);
  },

  // NOTE: throttling test must come last, since each test runs with a single
  // quota.
  function throttling() {
    // Test script is as so:
    //   1 - storage.local shouldn't be exceeded.
    //   2 - storage.sync should be exceeded.
    //   3 - storage.local still shouldn't be exceeded.
    //   4 - storage.sync should still be exceeded.
    //
    // In general, things should get throttled after 1000 calls (though in
    // reality will be fewer due to previous tests).

    function clearNTimes(area, n, whenDone) {
      if (n <= 0) {
        whenDone();
      } else {
        area.clear(function() {
          clearNTimes(area, n - 1, whenDone);
        });
      }
    }

    var local = chrome.storage.local;
    var sync = chrome.storage.sync;
    var test = chrome.test;
    var quotaError =
        "This request exceeds the MAX_WRITE_OPERATIONS_PER_HOUR quota.";

    clearNTimes(local, 1001, test.callbackPass(function() {
      clearNTimes(sync, 1001, test.callbackFail(quotaError, function() {
        clearNTimes(local, 1, test.callbackPass(function() {
          clearNTimes(sync, 1, test.callbackFail(quotaError, test.succeed));
        }));
      }));
    }));
  },
]);
