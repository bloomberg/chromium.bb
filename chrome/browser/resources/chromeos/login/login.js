// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Login UI based on a stripped down OOBE controller.
 */

<include src="login_common.js">
<include src="notification_card.js">

cr.define('cr.ui.Oobe', function() {
  return {
    /**
     * Initializes the OOBE flow.  This will cause all C++ handlers to
     * be invoked to do final setup.
     */
    initialize: function() {
      // TODO(jdufault): Remove this after resolving crbug.com/452599.
      console.log('Initializing OOBE');

      cr.ui.login.DisplayManager.initialize();
      login.WrongHWIDScreen.register();
      login.AccountPickerScreen.register();
      login.GaiaSigninScreen.register();
      login.UserImageScreen.register(/* lazyInit= */ true);
      login.ResetScreen.register();
      login.AutolaunchScreen.register();
      login.KioskEnableScreen.register();
      login.ErrorMessageScreen.register();
      login.TPMErrorMessageScreen.register();
      login.PasswordChangedScreen.register();
      login.SupervisedUserCreationScreen.register();
      login.TermsOfServiceScreen.register();
      login.AppLaunchSplashScreen.register();
      login.ConfirmPasswordScreen.register();
      login.FatalErrorScreen.register();
      login.DeviceDisabledScreen.register();

      cr.ui.Bubble.decorate($('bubble'));
      login.HeaderBar.decorate($('login-header-bar'));

      chrome.send('screenStateInitialize');
    },

    // Dummy Oobe functions not present with stripped login UI.
    initializeA11yMenu: function(e) {},
    handleAccessibilityLinkClick: function(e) {},
    handleSpokenFeedbackClick: function(e) {},
    handleHighContrastClick: function(e) {},
    handleScreenMagnifierClick: function(e) {},
    setUsageStats: function(checked) {},
    setOemEulaUrl: function(oemEulaUrl) {},
    setTpmPassword: function(password) {},
    refreshA11yInfo: function(data) {},

    /**
     * Reloads content of the page.
     * @param {!Object} data New dictionary with i18n values.
     */
    reloadContent: function(data) {
      loadTimeData.overrideValues(data);
      i18nTemplate.process(document, loadTimeData);
      Oobe.getInstance().updateLocalizedContent_();
    },
  };
});
