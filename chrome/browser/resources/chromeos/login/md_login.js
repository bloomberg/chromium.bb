// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Login UI based on a stripped down OOBE controller.
 */

// <include src="test_util.js">
// <include src="../../../../../ui/login/screen.js">
// <include src="../../../../../ui/login/bubble.js">
// <include src="../../../../../ui/login/display_manager.js">
// <include src="demo_mode_test_helper.js">

// <include
// src="../../../../../ui/login/account_picker/chromeos_screen_account_picker.js">

// <include src="../../../../../ui/login/login_ui_tools.js">
// <include
// src="../../../../../ui/login/account_picker/chromeos_user_pod_row.js">
// <include src="cr_ui.js">
// <include src="oobe_screen_autolaunch.js">
// <include src="oobe_screen_supervision_transition.js">
// <include src="oobe_screen_assistant_optin_flow.js">
// <include src="oobe_select.js">

// <include src="screen_app_launch_splash.js">
// <include src="screen_arc_terms_of_service.js">
// <include src="screen_error_message.js">
// <include src="screen_password_changed.js">
// <include src="screen_tpm_error.js">
// <include src="screen_wrong_hwid.js">
// <include src="screen_confirm_password.js">
// <include src="screen_fatal_error.js">
// <include src="screen_device_disabled.js">
// <include src="screen_active_directory_password_change.js">
// <include src="screen_encryption_migration.js">
// <include src="screen_update_required.js">
// <include src="screen_sync_consent.js">
// <include src="screen_fingerprint_setup.js">
// <include src="screen_recommend_apps.js">
// <include src="screen_app_downloading.js">
// <include src="screen_discover.js">
// <include src="screen_multidevice_setup.js">

// <include src="../../gaia_auth_host/authenticator.js">
// <include src="notification_card.js">

/**
 * Ensures that the pin keyboard is loaded.
 * @param {object} onLoaded Callback executed when the pin keyboard is loaded.
 */
function ensurePinKeyboardLoaded(onLoaded) {
  'use strict';

  // Wait a frame before running |onLoaded| to avoid any visual glitches.
  setTimeout(onLoaded);
}

cr.define('cr.ui.Oobe', function() {
  return {
    /**
     * Initializes the OOBE flow.  This will cause all C++ handlers to
     * be invoked to do final setup.
     */
    initialize() {
      cr.ui.login.DisplayManager.initialize();
      login.WrongHWIDScreen.register();
      login.AccountPickerScreen.register();
      login.AutolaunchScreen.register();
      login.ErrorMessageScreen.register();
      login.TPMErrorMessageScreen.register();
      login.PasswordChangedScreen.register();
      login.SyncConsentScreen.register();
      login.FingerprintSetupScreen.register();
      login.ArcTermsOfServiceScreen.register();
      login.RecommendAppsScreen.register();
      login.AppDownloadingScreen.register();
      login.AppLaunchSplashScreen.register();
      login.ConfirmPasswordScreen.register();
      login.FatalErrorScreen.register();
      login.DeviceDisabledScreen.register();
      login.ActiveDirectoryPasswordChangeScreen.register(/* lazyInit= */ true);
      login.EncryptionMigrationScreen.register();
      login.SupervisionTransitionScreen.register();
      login.UpdateRequiredScreen.register();
      login.DiscoverScreen.register();
      login.AssistantOptInFlowScreen.register();
      login.MultiDeviceSetupScreen.register();

      cr.ui.Bubble.decorate($('bubble-persistent'));
      $('bubble-persistent').persistent = true;
      $('bubble-persistent').hideOnKeyPress = false;

      cr.ui.Bubble.decorate($('bubble'));

      // TODO(crbug.com/1082670): Remove excessive logging after investigation.
      console.warn('Login : Initialize');
      chrome.send('screenStateInitialize');
    },

    // Dummy Oobe functions not present with stripped login UI.
    setUsageStats(checked) {},
    setTpmPassword(password) {},
    refreshA11yInfo(data) {},
    reloadEulaContent(data) {},

    /**
     * Reloads content of the page.
     * @param {!Object} data New dictionary with i18n values.
     */
    reloadContent(data) {
      loadTimeData.overrideValues(data);
      i18nTemplate.process(document, loadTimeData);
      Oobe.getInstance().updateLocalizedContent_();
    },

    /**
     * Updates "device in tablet mode" state when tablet mode is changed.
     * @param {Boolean} isInTabletMode True when in tablet mode.
     */
    setTabletModeState(isInTabletMode) {
      Oobe.getInstance().setTabletModeState_(isInTabletMode);
    },

    /**
     * Updates OOBE configuration when it is loaded.
     * @param {!OobeTypes.OobeConfiguration} configuration OOBE configuration.
     */
    updateOobeConfiguration(configuration) {
      Oobe.getInstance().updateOobeConfiguration_(configuration);
    },
  };
});
