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

  // List tests for individual elements. The media-router-container tests are
  // split between several files and use common functionality from
  // media_router_container_test_base.js.
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'issue_banner_tests.js',
    'media_router_container_cast_mode_list_tests.js',
    'media_router_container_filter_tests.js',
    'media_router_container_first_run_flow_tests.js',
    'media_router_container_route_tests.js',
    'media_router_container_search_tests.js',
    'media_router_container_sink_list_tests.js',
    'media_router_container_test_base.js',
    'media_router_header_tests.js',
    'media_router_search_highlighter_tests.js',
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

    // This element is used as a focus placeholder on dialog open, then
    // deleted. The user will be unable to tab to it. Remove when there is a
    // long term fix.
    this.accessibilityAuditConfig.ignoreSelectors(
       'focusableElementNotVisibleAndNotAriaHidden', '#focus-placeholder');
  },
};

TEST_F('MediaRouterElementsBrowserTest', 'IssueBanner', function() {
  issue_banner.registerTests();
  mocha.run();
});

// The media-router-container tests are being split into multiple parts due to
// timeout issues on bots.
TEST_F('MediaRouterElementsBrowserTest',
    'MediaRouterContainerCastModeList',
    function() {
  media_router_container_cast_mode_list.registerTests();
  mocha.run();
});

TEST_F('MediaRouterElementsBrowserTest',
    'MediaRouterContainerFirstRunFlow',
    function() {
  media_router_container_first_run_flow.registerTests();
  mocha.run();
});

TEST_F('MediaRouterElementsBrowserTest',
    'MediaRouterContainerRoute',
    function() {
  media_router_container_route.registerTests();
  mocha.run();
});

TEST_F('MediaRouterElementsBrowserTest',
    'MediaRouterContainerSearch',
    function() {
  media_router_container_search.registerTests();
  mocha.run();
});

TEST_F('MediaRouterElementsBrowserTest',
    'MediaRouterContainerSinkList',
    function() {
  media_router_container_sink_list.registerTests();
  mocha.run();
});

TEST_F('MediaRouterElementsBrowserTest',
    'MediaRouterContainerFilter',
    function() {
  media_router_container_filter.registerTests();
  mocha.run();
});

TEST_F('MediaRouterElementsBrowserTest', 'MediaRouterHeader', function() {
  media_router_header.registerTests();
  mocha.run();
});

TEST_F('MediaRouterElementsBrowserTest',
    'MediaRouterSearchHighlighter',
    function() {
  media_router_search_highlighter.registerTests();
  mocha.run();
});

TEST_F('MediaRouterElementsBrowserTest', 'MediaRouterRouteDetails', function() {
  route_details.registerTests();
  mocha.run();
});
