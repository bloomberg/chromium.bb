// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
   * Browse to settings.
   */
  browsePreload: 'chrome://settings-frame',

  /** @inheritDoc */
  preLoad: function() {
    this.makeAndRegisterMockHandler(['stopSyncing',
                                     'SyncSetupDidClosePage',
                                     'SyncSetupSubmitAuth',
                                     'SyncSetupConfigure',
                                     'SyncSetupPassphrase',
                                     'SyncSetupPassphraseCancel',
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
    var startStopSyncButton = BrowserOptions.getStartStopSyncButton();
    assertNotEquals(null, startStopSyncButton);
    this.mockHandler.expects(once()).SyncSetupShowSetupUI().
        will(callFunction(function() {
                            OptionsPage.navigateToPage('syncSetup');
                          }));

    this.mockHandler.expects(once()).SyncSetupShowSetupUI().
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

/**
 * Async version of SyncSetupWebUITest.
 * @extends {SyncSetupWebUITest}
 * @constructor
 */
function SyncSetupWebUITestAsync() {}

SyncSetupWebUITestAsync.prototype = {
  __proto__: SyncSetupWebUITest.prototype,

  /** @inheritDoc */
  isAsync: true,
};

// Verify that initial state is unsynced, start syncing, then login.
// TODO(estade): this doesn't work. DidShowPage is called multiple times.
TEST_F('SyncSetupWebUITestAsync', 'DISABLED_VerifySignIn', function() {
  // Start syncing to pull up the sign in page.
  assertFalse(BrowserOptions.getInstance().syncSetupCompleted);
  this.startSyncing();

  // Verify the DOM objects on the page.
  var gaiaEmail = SyncSetupOverlay.getLoginEmail();
  assertNotEquals(null, gaiaEmail);
  var gaiaPasswd = SyncSetupOverlay.getLoginPasswd();
  assertNotEquals(null, gaiaPasswd);
  var signInButton = SyncSetupOverlay.getSignInButton();
  assertNotEquals(null, signInButton);

  // For testing, don't wait to execute timeouts.
  var oldSetTimeout = setTimeout;
  setTimeout = function(fn, timeout) {
    oldSetTimeout(fn, 0);
  };

  // Expect set up submission and close messages sent through chrome.send().
  this.mockHandler.expects(once()).SyncSetupSubmitAuth(NOT_NULL).
      will(callFunction(
          function() {
            var loginSuccess = localStrings.getString('loginSuccess');
            expectNotEquals(loginSuccess, signInButton.value);
            SyncSetupOverlay.showSuccessAndClose();
            expectEquals(loginSuccess, signInButton.value);
          }));
  // The test completes after the asynchronous close.
  this.mockHandler.expects(once()).SyncSetupDidClosePage().
      will(callFunction(testDone));

  // Set the email & password, then sign in.
  gaiaEmail.value = 'foo@bar.baz';
  gaiaPasswd.value = 'foo@bar.baz';
  signInButton.click();
});
