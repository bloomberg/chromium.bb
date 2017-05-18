// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common OOBE controller methods. This method is shared between
 * OOBE, login, and the lock screen. Add only methods that need to be shared
 * between all *three* screens here, as each additional method increases the
 * time it takes to show the lock screen.
 *
 * If a method needs to be shared between the oobe and login screens, add it to
 * login_non_lock_shared.js.
 */

// <include src="test_util.js">
// <include src="../../../../../ui/login/screen.js">
// <include src="screen_context.js">
// <include src="../user_images_grid.js">
// <include src="apps_menu.js">
// <include src="../../../../../ui/login/bubble.js">
// <include src="../../../../../ui/login/display_manager.js">
// <include src="md_header_bar.js">

// <include src="../../../../../ui/login/account_picker/md_screen_account_picker.js">

// <include src="../../../../../ui/login/login_ui_tools.js">
// <include src="../../../../../ui/login/account_picker/md_user_pod_row.js">
// <include src="../../../../../ui/login/resource_loader.js">

cr.define('cr.ui', function() {
  var DisplayManager = cr.ui.login.DisplayManager;

  /**
  * Constructs an Out of box controller. It manages initialization of screens,
  * transitions, error messages display.
  * @extends {DisplayManager}
  * @constructor
  */
  function Oobe() {
  }

  /**
   * Delay in milliseconds between start of OOBE animation and start of
   * header bar animation.
   */
  var HEADER_BAR_DELAY_MS = 300;

  cr.addSingletonGetter(Oobe);

  Oobe.prototype = {
    __proto__: DisplayManager.prototype,
  };

  /**
   * Handle accelerators. These are passed from native code instead of a JS
   * event handler in order to make sure that embedded iframes cannot swallow
   * them.
   * @param {string} name Accelerator name.
   */
  Oobe.handleAccelerator = function(name) {
    Oobe.getInstance().handleAccelerator(name);
  };

  /**
   * Shows the given screen.
   * @param {Object} screen Screen params dict, e.g. {id: screenId, data: data}
   */
  Oobe.showScreen = function(screen) {
    Oobe.getInstance().showScreen(screen);
  };

  /**
   * Updates missin API keys message visibility.
   * @param {boolean} show True if the message should be visible.
   */
  Oobe.showAPIKeysNotice = function(show) {
    $('api-keys-notice-container').hidden = !show;
  };

  /**
   * Updates version label visibility.
   * @param {boolean} show True if version label should be visible.
   */
  Oobe.showVersion = function(show) {
    Oobe.getInstance().showVersion(show);
  };

  /**
   * Update body class to switch between OOBE UI and Login UI.
   */
  Oobe.showOobeUI = function(showOobe) {
    if (showOobe) {
      document.body.classList.add('oobe-display');

      // Callback to animate the header bar in.
      var showHeaderBar = function() {
        login.HeaderBar.animateIn(false, function() {
          chrome.send('headerBarVisible');
        });
      };
      // Start asynchronously so the OOBE network screen comes in first.
      window.setTimeout(showHeaderBar, HEADER_BAR_DELAY_MS);
    } else {
      document.body.classList.remove('oobe-display');
      Oobe.getInstance().prepareForLoginDisplay_();
      // Ensure header bar is visible when switching to Login UI from oobe.
      if (Oobe.getInstance().displayType == DISPLAY_TYPE.OOBE)
        login.HeaderBar.animateIn(true);
    }

    Oobe.getInstance().headerHidden = false;
  };

  /**
   * When |showShutdown| is set to "true", the shutdown button is shown and the
   * reboot button hidden. If set to "false", the reboot button is visible and
   * the shutdown button hidden.
   */
  Oobe.showShutdown = function(showShutdown) {
    $('login-header-bar').showShutdownButton = showShutdown;
    $('login-header-bar').showRebootButton = !showShutdown;
  };

  /**
   * Enables keyboard driven flow.
   */
  Oobe.enableKeyboardFlow = function(value) {
    // Don't show header bar for OOBE.
    Oobe.getInstance().forceKeyboardFlow = value;
  };

  /**
   * Disables signin UI.
   */
  Oobe.disableSigninUI = function() {
    DisplayManager.disableSigninUI();
  };

  /**
   * Shows signin UI.
   * @param {string} opt_email An optional email for signin UI.
   */
  Oobe.showSigninUI = function(opt_email) {
    DisplayManager.showSigninUI(opt_email);
  };

  /**
   * Resets sign-in input fields.
   * @param {boolean} forceOnline Whether online sign-in should be forced.
   * If |forceOnline| is false previously used sign-in type will be used.
   */
  Oobe.resetSigninUI = function(forceOnline) {
    DisplayManager.resetSigninUI(forceOnline);
  };

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attemps tried.
   * @param {string} message Error message to show.
   * @param {string} link Text to use for help link.
   * @param {number} helpId Help topic Id associated with help link.
   */
  Oobe.showSignInError = function(loginAttempts, message, link, helpId) {
    DisplayManager.showSignInError(loginAttempts, message, link, helpId);
  };

  /**
   * Shows password changed screen that offers migration.
   * @param {boolean} showError Whether to show the incorrect password error.
   */
  Oobe.showPasswordChangedScreen = function(showError, email) {
    DisplayManager.showPasswordChangedScreen(showError, email);
  };

  /**
   * Shows dialog to create a supervised user.
   */
  Oobe.showSupervisedUserCreationScreen = function() {
    DisplayManager.showSupervisedUserCreationScreen();
  };

  /**
   * Shows TPM error screen.
   */
  Oobe.showTpmError = function() {
    DisplayManager.showTpmError();
  };

  /**
   * Shows Active Directory password change screen.
   * @param {string} username Name of the user that should change the password.
   */
  Oobe.showActiveDirectoryPasswordChangeScreen = function(username) {
    DisplayManager.showActiveDirectoryPasswordChangeScreen(username);
  };

  /**
   * Show user-pods.
   */
  Oobe.showUserPods = function() {
    $('pod-row').loadLastWallpaper();
    Oobe.showScreen({id: SCREEN_ACCOUNT_PICKER});
    Oobe.resetSigninUI(true);
  };

  /**
   * Clears error bubble as well as optional menus that could be open.
   */
  Oobe.clearErrors = function() {
    var accessibilityMenu = $('accessibility-menu');
    if (accessibilityMenu)
      accessibilityMenu.hide();
    DisplayManager.clearErrors();
  };

  /**
   * Displays animations on successful authentication, that have to happen
   * before login UI is dismissed.
   */
  Oobe.animateAuthenticationSuccess = function() {
    login.HeaderBar.animateOut(function() {
      chrome.send('unlockOnLoginSuccess');
    });
  };

  /**
   * Displays animations that have to happen once login UI is fully displayed.
   */
  Oobe.animateOnceFullyDisplayed = function() {
    login.HeaderBar.animateIn(true, function() {
      chrome.send('headerBarVisible');
    });
  };

  /**
   * Sets text content for a div with |labelId|.
   * @param {string} labelId Id of the label div.
   * @param {string} labelText Text for the label.
   */
  Oobe.setLabelText = function(labelId, labelText) {
    DisplayManager.setLabelText(labelId, labelText);
  };

  /**
   * Sets the text content of the enterprise info message.
   * If the text is empty, the entire notification will be hidden.
   * @param {string} messageText The message text.
   */
  Oobe.setEnterpriseInfo = function(messageText, assetId) {
    DisplayManager.setEnterpriseInfo(messageText, assetId);
  };

  /**
   * Sets the text content of the Bluetooth device info message.
   * @param {string} bluetoothName The Bluetooth device name text.
   */
  Oobe.setBluetoothDeviceInfo = function(bluetoothName) {
    DisplayManager.setBluetoothDeviceInfo(bluetoothName);
  };

  /**
   * Updates the device requisition string shown in the requisition prompt.
   * @param {string} requisition The device requisition.
   */
  Oobe.updateDeviceRequisition = function(requisition) {
    Oobe.getInstance().updateDeviceRequisition(requisition);
  };

  /**
   * Enforces focus on user pod of locked user.
   */
  Oobe.forceLockedUserPodFocus = function() {
    login.AccountPickerScreen.forceLockedUserPodFocus();
  };

  /**
   * Clears password field in user-pod.
   */
  Oobe.clearUserPodPassword = function() {
    DisplayManager.clearUserPodPassword();
  };

  /**
   * Restores input focus to currently selected pod.
   */
  Oobe.refocusCurrentPod = function() {
    DisplayManager.refocusCurrentPod();
  };

  /**
   * Some ForTesting APIs directly access to DOM. Because this script is loaded
   * in header, DOM tree may not be available at beginning.
   * In DOMContentLoaded, after Oobe.initialize() is done, this is marked to
   * true, indicating ForTesting methods can be called.
   * External script using ForTesting APIs should wait for this condition.
   * @type {boolean}
   */
  Oobe.readyForTesting = false;

  /**
   * Skip to login screen for telemetry.
   */
  Oobe.skipToLoginForTesting = function() {
    Oobe.disableSigninUI();
    chrome.send('skipToLoginForTesting');
  };

  /**
   * Login for telemetry.
   * @param {string} username Login username.
   * @param {string} password Login password.
   * @param {boolean} enterpriseEnroll Login as an enterprise enrollment?
   */
  Oobe.loginForTesting = function(username, password, gaia_id,
                                  enterpriseEnroll = false) {
    // Helper method that runs |fn| after |screenName| is visible.
    function waitForOobeScreen(screenName, fn) {
      if (Oobe.getInstance().currentScreen &&
          Oobe.getInstance().currentScreen.id === screenName) {
        fn();
      } else {
        $('oobe').addEventListener('screenchanged', function handler(e) {
          if (e.detail == screenName) {
            $('oobe').removeEventListener('screenchanged', handler);
            fn();
          }
        });
      }
    }

    Oobe.disableSigninUI();
    chrome.send('skipToLoginForTesting', [username]);

    if (!enterpriseEnroll) {
      chrome.send('completeLogin', [gaia_id, username, password, false]);
    } else {
      waitForOobeScreen('gaia-signin', function() {
        chrome.send('toggleEnrollmentScreen');
        chrome.send('toggleFakeEnrollment');
      });

      waitForOobeScreen('oauth-enrollment', function() {
        chrome.send('oauthEnrollCompleteLogin', [username, 'authcode']);
        chrome.send('completeLogin', [gaia_id, username, password, false]);
      });
    }
  };

  /**
   * Guest login for telemetry.
   */
  Oobe.guestLoginForTesting = function() {
    Oobe.skipToLoginForTesting();
    chrome.send('launchIncognito');
  };

  /**
   * Authenticate for telemetry - used for screenlocker.
   * @param {string} username Login username.
   * @param {string} password Login password.
   */
  Oobe.authenticateForTesting = function(username, password) {
    Oobe.disableSigninUI();
    chrome.send('authenticateUser', [username, password, false]);
  };

  /**
   * Gaia login screen for telemetry.
   */
  Oobe.addUserForTesting = function() {
    Oobe.skipToLoginForTesting();
    chrome.send('addUser');
  };

  /**
   * Shows the add user dialog. Used in browser tests.
   */
  Oobe.showAddUserForTesting = function() {
    chrome.send('showAddUser');
  };

  /**
   * Hotrod requisition for telemetry.
   */
  Oobe.remoraRequisitionForTesting = function() {
    chrome.send('setDeviceRequisition', ['remora']);
  };

  /**
   * Begin enterprise enrollment for telemetry.
   */
  Oobe.switchToEnterpriseEnrollmentForTesting = function() {
    chrome.send('toggleEnrollmentScreen');
  };

  /**
   * Finish enterprise enrollment for telemetry.
   */
  Oobe.enterpriseEnrollmentDone = function() {
    chrome.send('oauthEnrollClose', ['done']);
  };

  /**
   * Returns true if enrollment was successful. Dismisses the enrollment
   * attribute screen if it's present.
   */
  Oobe.isEnrollmentSuccessfulForTest = function() {
    if (document.querySelector('.oauth-enroll-state-attribute-prompt'))
      chrome.send('oauthEnrollAttributes', ['', '']);

    return $('oauth-enrollment').classList.contains(
      'oauth-enroll-state-success');
  };

  /**
   * Shows/hides login UI control bar with buttons like [Shut down].
   */
  Oobe.showControlBar = function(show) {
    Oobe.getInstance().headerHidden = !show;
  };

  /**
   * Changes some UI which depends on the virtual keyboard being shown/hidden.
   */
  Oobe.setVirtualKeyboardShown = function(shown) {
    Oobe.getInstance().virtualKeyboardShown = shown;
    $('pod-row').setFocusedPodPinVisibility(!shown);
  };

  /**
   * Sets the current size of the client area (display size).
   * @param {number} width client area width
   * @param {number} height client area height
   */
  Oobe.setClientAreaSize = function(width, height) {
    Oobe.getInstance().setClientAreaSize(width, height);
  };

  // Export
  return {
    Oobe: Oobe
  };
});

var Oobe = cr.ui.Oobe;

// Allow selection events on components with editable text (password field)
// bug (http://code.google.com/p/chromium/issues/detail?id=125863)
disableTextSelectAndDrag(function(e) {
  var src = e.target;
  return src instanceof HTMLTextAreaElement ||
         src instanceof HTMLInputElement &&
         /text|password|search/.test(src.type);
});


(function() {
  'use strict';

  document.addEventListener('DOMContentLoaded', function() {
    try {
      Oobe.initialize();
    } finally {
      // TODO(crbug.com/712078): Do not set readyForTesting in case of that
      // initialize() is failed. Currently, in some situation, initialize()
      // raises an exception unexpectedly. It means testing APIs should not
      // be called then. However, checking it here now causes bots failures
      // unfortunately. So, as a short term workaround, here set
      // readyForTesting even on failures, just to make test bots happy.
      Oobe.readyForTesting = true;
    }
  });

  // Install a global error handler so stack traces are included in logs.
  window.onerror = function(message, file, line, column, error) {
    console.error(error.stack);
  }
})();
