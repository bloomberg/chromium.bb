// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Common utilities for extension ui tests. */
cr.define('extension_test_util', function() {
  /**
   * A mock to test that clicking on an element calls a specific method.
   * @constructor
   */
  function ClickMock() {}

  ClickMock.prototype = {
    /**
     * Tests clicking on an element and expecting a call.
     * @param {HTMLElement} element The element to click on.
     * @param {string} callName The function expected to be called.
     * @param {Array<*>=} opt_expectedArgs The arguments the function is
     *     expected to be called with.
     */
    testClickingCalls: function(element, callName, opt_expectedArgs) {
      var mock = new MockController();
      var mockMethod = mock.createFunctionMock(this, callName);
      MockMethod.prototype.addExpectation.apply(
          mockMethod, opt_expectedArgs);
      MockInteractions.tap(element);
      mock.verifyMocks();
    },
  };

  /**
   * Tests that the element's visibility matches |expectedVisible| and,
   * optionally, has specific content if it is visible.
   * @param {!HTMLElement} parentEl The parent element to query for the element.
   * @param {string} selector The selector to find the element.
   * @param {boolean} expectedVisible Whether the element should be
   *     visible.
   * @param {string=} opt_expected The expected textContent value.
   */
  function testVisible(parentEl, selector, expectedVisible, opt_expected) {
    var element = parentEl.$$(selector);
    var rect = element ? element.getBoundingClientRect() : null;
    var isVisible = !!rect && rect.width * rect.height > 0;
    expectEquals(expectedVisible, isVisible, selector);
    if (expectedVisible && opt_expected && element)
      expectEquals(opt_expected, element.textContent, selector);
  }

  return {
    ClickMock: ClickMock,
    testVisible: testVisible,
  };
});
