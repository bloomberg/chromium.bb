// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "chrome/browser/ui/webui/options/' +
    'managed_user_settings_test.h"');

/**
 * Test fixture for managed user settings WebUI testing.
 * @constructor
 * @extends {testing.Test}
 */
function ManagedUserSettingsTest() {}

ManagedUserSettingsTest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the managed user settings page .
   */
  browsePreload: 'chrome://settings-frame/managedUser',

  /** @override */
  typedefCppFixture: 'ManagedUserSettingsTest',

  /** @override */
  runAccessibilityChecks: false,

};

// Verify that the settings page is locked and can be unlocked.
TEST_F('ManagedUserSettingsTest', 'PageLocked',
    function() {
      var instance = ManagedUserSettings.getInstance();
      expectFalse(instance.isAuthenticated);
      // Now verify that the unlock button can be clicked.
      var unlockButton =
          options.ManagedUserSettingsForTesting.getUnlockButton();
      expectFalse(unlockButton.disabled);
      unlockButton.click();
      // When closing the page, we expect the elevation to be reset.
      OptionsPage.closeOverlay();
    });
