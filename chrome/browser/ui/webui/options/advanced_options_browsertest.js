// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for advanced options WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function AdvancedOptionsWebUITest() {}

AdvancedOptionsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to advanced options.
   **/
  browsePreload: 'chrome://settings/advanced',

  /**
   * Register a mock handler.
   * @type {Function}
   * @override
   */
  preLoad: function() {
    this.makeAndRegisterMockHandler(['defaultZoomFactorAction']);
  },
};

/**
 * The expected minimum length of the |defaultZoomFactor| element.
 * @type {number}
 * @const
 */
var defaultZoomFactorMinimumLength = 10;

/**
 * Test opening the advanced options has correct location.
 */
TEST_F('AdvancedOptionsWebUITest', 'testOpenAdvancedOptions', function() {
  assertEquals(this.browsePreload, document.location.href);
});

/**
 * Test the default zoom factor select element.
 */
TEST_F('AdvancedOptionsWebUITest', 'testDefaultZoomFactor', function() {
  // Verify that the zoom factor element exists.
  var defaultZoomFactor = $('defaultZoomFactor');
  assertNotEquals(defaultZoomFactor, null);

  // Verify that the zoom factor element has a reasonable number of choices.
  expectGE(defaultZoomFactor.options.length, defaultZoomFactorMinimumLength);

  // Simulate a change event, selecting the highest zoom value.  Verify that
  // the javascript handler was invoked once.
  this.mockHandler.expects(once()).defaultZoomFactorAction(NOT_NULL).
      will(callFunction(function() { }));
  defaultZoomFactor.selectedIndex = defaultZoomFactor.options.length - 1;
  var event = { target: defaultZoomFactor };
  if (defaultZoomFactor.onchange) defaultZoomFactor.onchange(event);
});

