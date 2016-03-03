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

  /** @override */
  runAccessibilityChecks: true,

  /** @override */
  accessibilityIssuesAreErrors: true,

  commandLineSwitches: [{
    switchName: 'media-router', switchValue: '1'
  }],

  // List tests for individual elements. The media_router_container tests are
  // split between media_router_container_tests.js and
  // media_router_container_filter_tests.js.
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'issue_banner_tests.js',
    'media_router_container_tests.js',
    'media_router_container_filter_tests.js',
    'media_router_header_tests.js',
    'media_router_search_highlighter.js',
    'route_details_tests.js',
  ]),

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);

    // Enable when failure is resolved.
    // AX_ARIA_02: http://crbug.com/591547
    this.accessibilityAuditConfig.ignoreSelectors(
        'nonExistentAriaRelatedElement', '#input');

    // Enable when failure is resolved.
    // AX_ARIA_04: http://crbug.com/591550
    this.accessibilityAuditConfig.ignoreSelectors(
        'badAriaAttributeValue', '#input');

    var requiredOwnedAriaRoleMissingSelectors = [
      '#cast-mode-list',
      '#sink-list'
    ];

    // Enable when failure is resolved.
    // AX_ARIA_08: http://crbug.com/591552
    this.accessibilityAuditConfig.ignoreSelectors(
        'requiredOwnedAriaRoleMissing', requiredOwnedAriaRoleMissingSelectors);

    // Enable when warning is resolved.
    // AX_FOCUS_01: http://crbug.com/591553
    this.accessibilityAuditConfig.ignoreSelectors(
        'focusableElementNotVisibleAndNotAriaHidden', '#device-missing > A');

    // Enable when warning is resolved.
    // AX_TEXT_04: http://crbug.com/591554
    this.accessibilityAuditConfig.ignoreSelectors(
        'linkWithUnclearPurpose', '#device-missing > A');
  },
};

TEST_F('MediaRouterElementsBrowserTest', 'MediaRouterElementsTestIssueBanner',
    function() {
  // Register mocha tests for the issue banner.
  issue_banner.registerTests();

  // Run all registered tests.
  mocha.run();
});

// See bugs.chromium.org issue 591227
TEST_F('MediaRouterElementsBrowserTest',
    'DISABLED_MediaRouterElementsTestMediaRouterContainer',
    function() {
  // Register mocha tests for the container.
  media_router_container.registerTests();

  // Run all registered tests.
  mocha.run();
});

TEST_F('MediaRouterElementsBrowserTest',
    'MediaRouterElementsTestMediaRouterContainerFilter',
    function() {
  // Register mocha tests for the container filter.
  media_router_container_filter.registerTests();

  // Run all registered tests.
  mocha.run();
});

TEST_F('MediaRouterElementsBrowserTest',
    'MediaRouterElementsTestMediaRouterHeader',
    function() {
  // Register mocha tests for the header.
  media_router_header.registerTests();

  // Run all registered tests.
  mocha.run();
});

TEST_F('MediaRouterElementsBrowserTest',
    'MediaRouterElementsTestMediaRouterSearchHighlighter',
    function() {
  // Register mocha tests for the search highlighter.
  media_router_search_highlighter.registerTests();

  // Run all registered tests.
  mocha.run();
});

TEST_F('MediaRouterElementsBrowserTest',
    'MediaRouterElementsTestMediaRouterRouteDetails',
    function() {
  // Register mocha tests for the route details.
  route_details.registerTests();

  // Run all registered tests.
  mocha.run();
});
