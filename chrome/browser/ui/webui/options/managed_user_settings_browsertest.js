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

  /** @inheritDoc */
  preLoad: function() {
    this.makeAndRegisterMockHandler(['displayPassphraseDialog']);
  },

};

// Verify that the settings page is locked when a passphrase is specified.
TEST_F('ManagedUserSettingsTest', 'PageLocked',
    function() {
      // Check that the user is not authenticated when a passphrase is set.
      ManagedUserSettings.initializeSetPassphraseButton(true);
      var instance = ManagedUserSettings.getInstance();
      expectEquals(instance.authenticationState,
                   options.ManagedUserAuthentication.UNAUTHENTICATED);
      // Now verify that the unlock button can be clicked.
      var unlockButton =
          options.ManagedUserSettingsForTesting.getUnlockButton();
      expectFalse(unlockButton.disabled);
      this.mockHandler.expects((once())).displayPassphraseDialog(
          ['options.ManagedUserSettings.isAuthenticated']);
      unlockButton.click();
    });

// Verify that the settings page is not locked when no passphrase is specified.
TEST_F('ManagedUserSettingsTest', 'PageUnlocked',
    function() {
      var instance = ManagedUserSettings.getInstance();
      expectEquals(instance.authenticationState,
                   options.ManagedUserAuthentication.AUTHENTICATED);
      var unlockButton =
          options.ManagedUserSettingsForTesting.getUnlockButton();
      expectTrue(unlockButton.disabled);
    });
