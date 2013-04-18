// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "chrome/browser/ui/webui/options/' +
    'managed_user_set_passphrase_test.h"');

/**
 * Test fixture for managed user set passphrase WebUI testing.
 * @constructor
 * @extends {testing.Test}
 */
function ManagedUserSetPassphraseTest() {}

ManagedUserSetPassphraseTest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the managed user settings page .
   */
  browsePreload: 'chrome://settings-frame/managedUser',

  /** @override */
  typedefCppFixture: 'ManagedUserSetPassphraseTest',

  /** @inheritDoc */
  preLoad: function() {
    this.makeAndRegisterMockHandler(['setPassphrase']);
  },

  /**
   * Clicks the "Set passphrase" button.
   */
  setPassphrase: function() {
    var setPassphraseButton =
        options.ManagedUserSettingsForTesting.getSetPassphraseButton();
    assertNotEquals(null, setPassphraseButton);
    setPassphraseButton.click();
  },

  /**
   * Enters a passphrase.
   */
  enterPassphrase: function(passphrase) {
    var passphraseInput =
        options.ManagedUserSetPassphraseForTesting.getPassphraseInput();
    assertNotEquals(null, passphraseInput);
    passphraseInput.value = passphrase;
  },

  /**
   * Confirms the passphrase.
   */
  confirmPassphrase: function(passphrase) {
    var passphraseConfirmInput =
        options.ManagedUserSetPassphraseForTesting.getPassphraseConfirmInput();
    assertNotEquals(null, passphraseConfirmInput);
    passphraseConfirmInput.value = passphrase;
  },

  /**
   * Save the passphrase.
   */
  savePassphrase: function() {
    var testObject = options.ManagedUserSetPassphraseForTesting;
    var savePassphraseButton = testObject.getSavePassphraseButton();
    assertNotEquals(null, savePassphraseButton);
    // This takes care of enabling the save passphrase button (if applicable).
    testObject.updateDisplay();
    savePassphraseButton.click();
  },
};

// Verify that the save passphrase flow works.
TEST_F('ManagedUserSetPassphraseTest', 'SamePassphrase',
    function() {
      this.setPassphrase();
      this.enterPassphrase('test_passphrase');
      this.confirmPassphrase('test_passphrase');
      this.mockHandler.expects(once()).setPassphrase(['test_passphrase']);
      this.savePassphrase();
    });

// Verify that empty passphrase is not allowed.
TEST_F('ManagedUserSetPassphraseTest', 'EmptyPassphrase',
    function() {
      this.setPassphrase();
      this.enterPassphrase('');
      this.confirmPassphrase('');
      this.mockHandler.expects(once()).setPassphrase(['']);
      this.savePassphrase();
    });

// Verify that the confirm passphrase input needs to contain the same
// passphrase as the passphrase input.
TEST_F('ManagedUserSetPassphraseTest', 'DifferentPassphrase',
    function() {
      this.setPassphrase();
      this.enterPassphrase('test_passphrase');
      this.confirmPassphrase('confirm_passphrase');
      this.mockHandler.expects(never()).setPassphrase(ANYTHING);
      this.savePassphrase();
    });
