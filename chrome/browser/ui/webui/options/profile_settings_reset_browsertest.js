// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['options_browsertest_base.js']);

/**
 * TestFixture for profile settings reset WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function ProfileSettingsResetWebUITest() {}

ProfileSettingsResetWebUITest.prototype = {
  __proto__: OptionsBrowsertestBase.prototype,

  /**
   * Browse to the reset profile settings page.
   */
  browsePreload: 'chrome://settings-frame/resetProfileSettings',

  /** @override */
  setUp: function() {
    OptionsBrowsertestBase.prototype.setUp.call(this);

    // Enable when failure is resolved.
    // AX_TEXT_04: http://crbug.com/570551
    this.accessibilityAuditConfig.ignoreSelectors(
        'linkWithUnclearPurpose',
        '#reset-profile-settings-overlay > .action-area > .hbox.stretch > A');
  },
};

// Test opening the profile settings reset has correct location.
TEST_F('ProfileSettingsResetWebUITest', 'testOpenProfileSettingsReset',
       function() {
         assertEquals(this.browsePreload, document.location.href);
       });
