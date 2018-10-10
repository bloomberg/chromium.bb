// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Subpage of settings-multidevice-feature for managing the Smart Lock feature.
 */
cr.exportPath('settings');

cr.define('settings', function() {
  /**
   * The state of the preference controlling Smart Lock's ability to sign-in the
   * user.
   * @enum {string}
   */
  SignInEnabledState = {
    ENABLED: 'enabled',
    DISABLED: 'disabled',
  };

  /** @const {string} */
  SmartLockSignInEnabledPrefName = 'proximity_auth.is_chromeos_login_enabled';

  return {
    SignInEnabledState: SignInEnabledState,
        SmartLockSignInEnabledPrefName: SmartLockSignInEnabledPrefName,
  };
});

Polymer({
  is: 'settings-multidevice-smartlock-subpage',

  behaviors: [
    MultiDeviceFeatureBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @type {?SettingsRoutes} */
    routes: {
      type: Object,
      value: settings.routes,
    },

    /**
     * True if Smart Lock is enabled.
     * @private
     */
    smartLockEnabled_: {
      type: Boolean,
      computed: 'computeIsSmartLockEnabled_(pageContentData)',
    },

    /**
     * Whether Smart Lock may be used to sign-in the user (as opposed to only
     * being able to unlock the user's screen).
     * @private {!settings.SignInEnabledState}
     */
    smartLockSignInEnabled_: {
      type: Object,
      value: settings.SignInEnabledState.DISABLED,
    },
  },

  /** @private {?settings.MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.MultiDeviceBrowserProxyImpl.getInstance();

    this.addWebUIListener(
        'smart-lock-signin-enabled-changed',
        this.updateSmartLockSignInEnabled_.bind(this));

    this.browserProxy_.getSmartLockSignInEnabled().then(enabled => {
      this.updateSmartLockSignInEnabled_(enabled);
    });
  },

  /**
   * Returns true if Smart Lock is an enabled feature.
   * @return {boolean}
   * @private
   */
  computeIsSmartLockEnabled_: function() {
    return !!this.pageContentData &&
        this.getFeatureState(settings.MultiDeviceFeature.SMART_LOCK) ==
        settings.MultiDeviceFeatureState.ENABLED_BY_USER;
  },

  /**
   * Updates the state of the Smart Lock 'sign-in enabled' toggle.
   * @private
   */
  updateSmartLockSignInEnabled_: function(enabled) {
    this.smartLockSignInEnabled_ = enabled ?
        settings.SignInEnabledState.ENABLED :
        settings.SignInEnabledState.DISABLED;
  },
});
