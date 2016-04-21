// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['options_browsertest_base.js']);

/**
 * TestFixture for cookies view WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function CookiesViewWebUITest() {}

CookiesViewWebUITest.prototype = {
  __proto__: OptionsBrowsertestBase.prototype,

  /**
   * Browse to the cookies view.
   */
  browsePreload: 'chrome://settings-frame/cookies',

  /** @override */
  setUp: function() {
    OptionsBrowsertestBase.prototype.setUp.call(this);

    // Enable when failure is resolved.
    // AX_TEXT_01: http://crbug.com/570560
    this.accessibilityAuditConfig.ignoreSelectors(
        'controlsWithoutLabel',
        '#cookies-view-page > .content-area.cookies-list-content-area > *');

    var requiredOwnedAriaRoleMissingSelectors = [
        '#default-search-engine-list',
        '#other-search-engine-list',
    ];

    // Enable when failure is resolved.
    // AX_ARIA_08: http://crbug.com/605689
    this.accessibilityAuditConfig.ignoreSelectors(
        'requiredOwnedAriaRoleMissing',
        requiredOwnedAriaRoleMissingSelectors);
  },
};

// Test opening the cookies view has correct location.
TEST_F('CookiesViewWebUITest', 'testOpenCookiesView', function() {
  assertEquals(this.browsePreload, document.location.href);
});

TEST_F('CookiesViewWebUITest', 'testNoCloseOnSearchEnter', function() {
  var cookiesView = CookiesView.getInstance();
  assertTrue(cookiesView.visible);
  var searchBox = cookiesView.pageDiv.querySelector('.cookies-search-box');
  searchBox.dispatchEvent(new KeyboardEvent('keydown', {
    'bubbles': true,
    'cancelable': true,
    'keyIdentifier': 'Enter'
  }));
  assertTrue(cookiesView.visible);
});
