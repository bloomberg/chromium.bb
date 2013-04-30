// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Login UI based on a stripped down OOBE controller.
 */

<include src="login_common.js"></include>

cr.define('cr.ui.Oobe', function() {
  return {
    /**
     * Initializes the OOBE flow.  This will cause all C++ handlers to
     * be invoked to do final setup.
     */
    initialize: function() {
      cr.ui.login.DisplayManager.initialize();
      login.WrongHWIDScreen.register();
      login.AccountPickerScreen.register();
      login.GaiaSigninScreen.register();
      login.UserImageScreen.register(/* lazyInit= */ true);
      login.ResetScreen.register();
      login.ErrorMessageScreen.register();
      login.TPMErrorMessageScreen.register();
      login.PasswordChangedScreen.register();
      login.LocallyManagedUserCreationScreen.register();
      login.TermsOfServiceScreen.register();

      cr.ui.Bubble.decorate($('bubble'));
      login.HeaderBar.decorate($('login-header-bar'));

      chrome.send('screenStateInitialize');
    },

    // Dummy Oobe functions not present with stripped login UI.
    initializeA11yMenu: function(e) {},
    handleAccessbilityLinkClick: function(e) {},
    handleSpokenFeedbackClick: function(e) {},
    handleHighContrastClick: function(e) {},
    handleScreenMagnifierClick: function(e) {},
    enableContinueButton: function(enable) {},
    setUsageStats: function(checked) {},
    setOemEulaUrl: function(oemEulaUrl) {},
    setUpdateProgress: function(progress) {},
    showUpdateEstimatedTimeLeft: function(enable) {},
    setUpdateEstimatedTimeLeft: function(seconds) {},
    setUpdateMessage: function(message) {},
    showUpdateCurtain: function(enable) {},
    setTpmPassword: function(password) {},
    refreshA11yInfo: function(data) {},
    reloadContent: function(data) {},
  };
});
