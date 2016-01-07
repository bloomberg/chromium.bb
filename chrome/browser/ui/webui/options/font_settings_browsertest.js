// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['options_browsertest_base.js']);

/**
 * TestFixture for font settings WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function FontSettingsWebUITest() {}

FontSettingsWebUITest.prototype = {
  __proto__: OptionsBrowsertestBase.prototype,

  /**
   * Browse to the font settings page.
   */
  browsePreload: 'chrome://settings-frame/fonts',

  /** @override */
  preLoad: function() {
    this.makeAndRegisterMockHandler(['openAdvancedFontSettingsOptions']);
  },

  /** @override */
  setUp: function() {
    OptionsBrowsertestBase.prototype.setUp.call(this);

    var controlsWithoutLabelSelectors = [
      '#standard-font-size',
      '#minimum-font-size',
    ];

    // Enable when failure is resolved.
    // AX_TEXT_01: http://crbug.com/570555
    this.accessibilityAuditConfig.ignoreSelectors(
        'controlsWithoutLabel',
        controlsWithoutLabelSelectors);
  },
};

// Test opening font settings has correct location.
TEST_F('FontSettingsWebUITest', 'testOpenFontSettings', function() {
  assertEquals(this.browsePreload, document.location.href);
});

// Test setup of the Advanced Font Settings links.
TEST_F('FontSettingsWebUITest', 'testAdvancedFontSettingsLink', function() {
  var installElement = $('advanced-font-settings-install');
  var optionsElement = $('advanced-font-settings-options');
  var expectedUrl = 'https://chrome.google.com/webstore/detail/' +
      'caclkomlalccbpcdllchkeecicepbmbm';

  FontSettings.notifyAdvancedFontSettingsAvailability(false);
  assertFalse(installElement.hidden);
  assertEquals(expectedUrl, installElement.querySelector('a').href);
  assertTrue(optionsElement.hidden);

  FontSettings.notifyAdvancedFontSettingsAvailability(true);
  assertTrue(installElement.hidden);
  assertFalse(optionsElement.hidden);
  this.mockHandler.expects(once()).openAdvancedFontSettingsOptions();
  optionsElement.click();
});
