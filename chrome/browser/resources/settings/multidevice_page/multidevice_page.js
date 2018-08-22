// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings page for managing MultiDevice features.
 */
cr.exportPath('settings');

Polymer({
  is: 'settings-multidevice-page',

  behaviors: [MultiDeviceFeatureBehavior],

  properties: {
    /**
     * A Map specifying which element should be focused when exiting a subpage.
     * The key of the map holds a settings.Route path, and the value holds a
     * query selector that identifies the desired element.
     * @private {!Map<string, string>}
     */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.MULTIDEVICE_FEATURES)
          map.set(
              settings.routes.MULTIDEVICE_FEATURES.path,
              '#multidevice-item .subpage-arrow');
        return map;
      },
    },

    /**
     * Authentication token provided by password-prompt-dialog.
     * @private {string}
     */
    authToken_: {
      type: String,
      value: '',
      observer: 'authTokenChanged_',
    },

    /**
     * The feature which the user wishes to enable, but which requires
     * authentication. Once |authToken_| is valid, this value will be passed
     * along to |browserProxy_|, and subsequently cleared.
     * @private {?settings.MultiDeviceFeature}
     */
    attemptedEnabledFeature_: {type: Number, value: null},

    /** @private {boolean} */
    showPasswordPromptDialog_: {type: Boolean, value: false},
  },

  listeners: {
    'feature-toggle-clicked': 'onFeatureToggleClicked_',
  },

  /** @private {?settings.MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.MultiDeviceBrowserProxyImpl.getInstance();
  },

  /**
   * @return {string} Translated item label.
   * @private
   */
  getLabelText_: function() {
    return this.pageContentData.hostDeviceName ||
        this.i18n('multideviceSetupItemHeading');
  },

  /**
   * @return {string} Translated sublabel with a "learn more" link.
   * @private
   */
  getSubLabelInnerHtml_: function() {
    switch (this.pageContentData.mode) {
      case settings.MultiDeviceSettingsMode.NO_HOST_SET:
        return this.i18nAdvanced('multideviceSetupSummary');
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER:
        return this.i18nAdvanced('multideviceCouldNotConnect');
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION:
        return this.i18nAdvanced('multideviceVerificationText');
      default:
        return this.isSuiteOn() ? this.i18n('multideviceEnabled') :
                                  this.i18n('multideviceDisabled');
    }
  },

  /**
   * @return {string} Translated button text.
   * @private
   */
  getButtonText_: function() {
    switch (this.pageContentData.mode) {
      case settings.MultiDeviceSettingsMode.NO_HOST_SET:
        return this.i18n('multideviceSetupButton');
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER:
        return this.i18n('retry');
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION:
        return this.i18n('multideviceVerifyButton');
      default:
        return '';
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  showButton_: function() {
    return this.pageContentData.mode !=
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  },

  /**
   * @return {boolean}
   * @private
   */
  showToggle_: function() {
    return this.pageContentData.mode ==
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  },

  /**
   * @return {boolean}
   * @private
   */
  doesClickOpenSubpage_: function() {
    return [
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
      settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED,
    ].includes(this.pageContentData.mode);
  },

  /** @private */
  handleItemClick_: function() {
    if (!this.doesClickOpenSubpage_())
      return;
    settings.navigateTo(settings.routes.MULTIDEVICE_FEATURES);
  },

  /** @private */
  handleButtonClick_: function(event) {
    event.stopPropagation();
    switch (this.pageContentData.mode) {
      case settings.MultiDeviceSettingsMode.NO_HOST_SET:
        this.browserProxy_.showMultiDeviceSetupDialog();
        return;
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER:
      // Intentional fall-through.
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION:
        // If this device is waiting for action on the server or the host
        // device, clicking the button should trigger this action.
        this.browserProxy_.retryPendingHostSetup();
    }
  },

  /**
   * Tell |this.browserProxy_| to enable |this.attemptedEnabledFeature_|.
   * @private
   */
  enableAttemptedFeature_: function() {
    if (this.attemptedEnabledFeature_ != null) {
      this.browserProxy_.setFeatureEnabledState(
          this.attemptedEnabledFeature_, true /* enabled */, this.authToken_);
      this.attemptedEnabledFeature_ = null;
    }
  },

  /** @private */
  openPasswordPromptDialog_: function() {
    this.showPasswordPromptDialog_ = true;
  },

  /** @private */
  onPasswordPromptDialogClose_: function() {
    this.showPasswordPromptDialog_ = false;
  },

  /**
   * Attempt to enable the provided feature. If not authenticated (i.e.,
   * |authToken_| is invalid), display the password prompt to begin the
   * authentication process.
   *
   * @param {!{detail: !Object}} event
   * @private
   */
  onFeatureToggleClicked_: function(event) {
    let feature = event.detail.feature;
    let enabled = event.detail.enabled;

    // Authentication is never required if disabling a feature; only consider it
    // if enabling.
    if (enabled) {
      // TODO(crbug.com/876436): Actually determine this, when the API is
      // available. It's fine to default to true here for now, because it errs
      // on the side of more security.
      let isSmartLockPrefEnabled = true;

      let isPasswordPromptRequired =
          (feature === settings.MultiDeviceFeature.SMART_LOCK) ||
          (feature === settings.MultiDeviceFeature.BETTER_TOGETHER_SUITE &&
           isSmartLockPrefEnabled);

      if (isPasswordPromptRequired) {
        this.attemptedEnabledFeature_ = feature;

        if (this.authToken_) {
          this.enableAttemptedFeature_();
        } else {
          this.openPasswordPromptDialog_();
        }
        return;
      }
    }

    // No authentication is required.
    this.browserProxy_.setFeatureEnabledState(feature, enabled);
  },

  /**
   * Called when the authToken_ changes. If the authToken is valid, that
   * indicates the user authenticated successfully. If not, cancel the pending
   * attempt to enable attemptedEnabledFeature_.
   * @param {String} authToken
   * @private
   */
  authTokenChanged_: function(authToken) {
    if (this.authToken_) {
      this.enableAttemptedFeature_();
    } else {
      this.attemptedEnabledFeature_ = null;
    }
  },
});
