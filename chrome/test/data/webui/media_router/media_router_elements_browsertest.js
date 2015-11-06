// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Media Router Polymer elements tests. */

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * Test fixture for Media Router Polymer elements.
 * @constructor
 * @extends {PolymerTest}
*/
function MediaRouterElementsBrowserTest() {}

MediaRouterElementsBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://media-router/',

  commandLineSwitches: [{
    switchName: 'media-router', switchValue: '1'
  }],

  // List tests for individual elements.
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'issue_banner_tests.js',
    'media_router_container_tests.js',
    'media_router_header_tests.js',
    'route_details_tests.js',
  ]),

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);

    // TODO(apacible): Enable when false positive AX_FOCUS_01 bug is fixed.
    // https://github.com/GoogleChrome/accessibility-developer-tools/issues/171
    this.accessibilityAuditConfig.auditRulesToIgnore.push(
        'focusableElementNotVisibleAndNotAriaHidden');
  },
};

// Runs all tests.
TEST_F('MediaRouterElementsBrowserTest', 'MediaRouterElementsTest', function() {
  // Register mocha tests for each element.
  issue_banner.registerTests();
  media_router_container.registerTests();
  media_router_header.registerTests();
  route_details.registerTests();

  // Run all registered tests.
  mocha.run();
});
