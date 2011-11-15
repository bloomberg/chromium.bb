// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for sync setup WebUI testing.
 * @constructor
 * @extends {testing.Test}
 */
function SyncSetupWebUITest() {}

SyncSetupWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to personal options.
   **/
  browsePreload: 'chrome://settings/personal',

  /** @inheritDoc */
  preLoad: function() {
    this.makeAndRegisterMockHandler(['stopSyncing',
                                     'SyncSetupDidClosePage',
                                     'SyncSetupSubmitAuth',
                                     'SyncSetupConfigure',
                                     'SyncSetupPassphrase',
                                     'SyncSetupPassphraseCancel',
                                     'SyncSetupAttachHandler',
                                     'SyncSetupShowErrorUI',
                                     'SyncSetupShowSetupUI',
                                    ]);
  },

  /**
   * Verifies starting point is not synced.
   */
  verifyUnsynced: function() {
  },

  /**
   * Clicks the "Sign in to Chrome" button.
   */
  startSyncing: function() {
    var startStopSyncButton = PersonalOptions.getStartStopSyncButton();
    assertNotEquals(null, startStopSyncButton);
    this.mockHandler.expects(once()).SyncSetupShowSetupUI().
        will(callFunction(function() {
                            OptionsPage.navigateToPage('syncSetup');
                          }));

    this.mockHandler.expects(once()).SyncSetupAttachHandler().
        will(callFunction(function() {
                            SyncSetupOverlay.showSyncSetupPage(
                                'login', {
                                  user: '',
                                  error: 0,
                                  editable_user: true,
                                });
                          }));
    startStopSyncButton.click();
  },
};

// Verify that initial state is unsynced, start syncing, then login.
TEST_F('SyncSetupWebUITest', 'VerifySignIn', function() {
  // Start syncing to pull up the sign in page.
  assertFalse(PersonalOptions.getInstance().syncSetupCompleted);
  this.startSyncing();

  // Verify the DOM objects on the page.
  var gaiaEmail = SyncSetupOverlay.getLoginEmail();
  assertNotEquals(null, gaiaEmail);
  var gaiaPasswd = SyncSetupOverlay.getLoginPasswd();
  assertNotEquals(null, gaiaPasswd);
  var signInButton = SyncSetupOverlay.getSignInButton();
  assertNotEquals(null, signInButton);

  // Expect set up submission and close messages sent through chrome.send().
  this.mockHandler.expects(once()).SyncSetupSubmitAuth(NOT_NULL).
      will(callFunction(function() {
                          SyncSetupOverlay.showSuccessAndClose();
                        }));
  this.mockHandler.expects(once()).SyncSetupDidClosePage();

  // Set the email & password, then sign in.
  gaiaEmail.value = 'foo@bar.baz';
  gaiaPasswd.value = 'foo@bar.baz';
  signInButton.click();
});

