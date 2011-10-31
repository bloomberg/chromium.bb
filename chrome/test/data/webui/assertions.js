// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function WebUIAssertionsTest() {}

WebUIAssertionsTest.prototype = {
  __proto__: testing.Test.prototype,
  browsePreload: DUMMY_URL,
};

function testTwoExpects() {
  expectTrue(false);
  expectTrue(0);
}

TEST_F('WebUIAssertionsTest', 'testTwoExpects', function() {
  var result = runTestFunction('testTwoExpects', testTwoExpects, []);
  resetTestState();

  expectFalse(result[0]);
  expectTrue(!!result[1].match(/expectTrue\(false\): false/));
  expectTrue(!!result[1].match(/expectTrue\(0\): 0/));
});

function twoExpects() {
  expectTrue(false, 'message1');
  expectTrue(false, 'message2');
}

function testCallTestTwice() {
  twoExpects();
  twoExpects();
}

TEST_F('WebUIAssertionsTest', 'testCallTestTwice', function() {
  var result = runTestFunction('testCallTestTwice', testCallTestTwice, []);
  resetTestState();

  expectFalse(result[0]);
  expectEquals(2, result[1].match(
      /expectTrue\(false, 'message1'\): message1: false/g).length);
  expectEquals(2, result[1].match(
      /expectTrue\(false, 'message2'\): message2: false/g).length);
});

function testConstructMessage() {
  var message = 1 + ' ' + 2;
  assertTrue(false, message);
}

TEST_F('WebUIAssertionsTest', 'testConstructedMessage', function() {
  var result = runTestFunction(
      'testConstructMessage', testConstructMessage, []);
  resetTestState();

  expectEquals(
      1, result[1].match(/assertTrue\(false, message\): 1 2: false/g).length);
});

/**
 * Failing version of WebUIAssertionsTest.
 * @extends WebUIAssertionsTest
 * @constructor
 */
function WebUIAssertionsTestFail() {}

WebUIAssertionsTestFail.prototype = {
  __proto__: WebUIAssertionsTest.prototype,

  /** @inheritDoc */
  testShouldFail: true,
};

// Test that an assertion failure fails test.
TEST_F('WebUIAssertionsTestFail', 'testAssertFailFails', function() {
  assertNotReached();
});

// Test that an expect failure fails test.
TEST_F('WebUIAssertionsTestFail', 'testExpectFailFails', function() {
  expectNotReached();
});

/**
 * Async version of WebUIAssertionsTestFail.
 * @extends WebUIAssertionsTest
 * @constructor
 */
function WebUIAssertionsTestAsyncFail() {}

WebUIAssertionsTestAsyncFail.prototype = {
  __proto__: WebUIAssertionsTestFail.prototype,

  /** @inheritDoc */
  isAsync: true,
};

// Test that an assertion failure doesn't hang forever.
TEST_F('WebUIAssertionsTestAsyncFail', 'testAsyncFailCallsDone', function() {
  assertNotReached();
});
