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
    },

    /**
     * Features which the user has requested to be enabled but could not be
     * enabled immediately because they require that the user enter a password
     * first. This array is only set when the password prompt is visible; if the
     * user enters a correct password, the features are enabled, and if the user
     * closes the prompt without entering a correct password, these pending
     * requests are dropped.
     * @private {!Array<!settings.MultiDeviceFeature>}
     */
    featuresToBeEnabledOnceAuthenticated_: {
      type: Array,
      value: [],
    },

    /** @private {boolean} */
    showPasswordPromptDialog_: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'feature-toggle-clicked': 'onFeatureToggleClicked_',
    'forget-device-requested': 'onForgetDeviceRequested_',
  },

  /** @private {?settings.MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.MultiDeviceBrowserProxyImpl.getInstance();
  },

  /**
   * CSS class for the <div> containing all the text in the multidevice-item
   * <div>, i.e. the label and sublabel. If the host is set, the Better Together
   * icon appears so before the text (i.e. text div is 'middle' class).
   * @return {string}
   * @private
   */
  getMultiDeviceItemLabelBlockCssClass_: function() {
    return this.isHostSet() ? 'middle' : 'start';
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
      case settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS:
        return this.i18nAdvanced('multideviceNoHostText');
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
  shouldShowButton_: function() {
    return [
      settings.MultiDeviceSettingsMode.NO_HOST_SET,
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
    ].includes(this.pageContentData.mode);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowToggle_: function() {
    return this.pageContentData.mode ===
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  },

  /**
   * Whether to show the separator bar and, if the state calls for a chevron
   * (a.k.a. subpage arrow) routing to the subpage, the chevron.
   * @return {boolean}
   * @private
   */
  shouldShowSeparatorAndSubpageArrow_: function() {
    return this.pageContentData.mode !==
        settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS;
  },

  /**
   * @return {boolean}
   * @private
   */
  doesClickOpenSubpage_: function() {
    return this.isHostSet();
  },

  /** @private */
  handleItemClick_: function() {
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

  /** @private */
  openPasswordPromptDialog_: function() {
    this.showPasswordPromptDialog_ = true;
  },

  /** @private */
  onPasswordPromptDialogClose_: function() {
    // If |this.authToken_| is set when the dialog has been closed, this means
    // that the user entered the correct password into the dialog. Thus, send
    // all pending features to be enabled.
    if (this.authToken_) {
      this.featuresToBeEnabledOnceAuthenticated_.forEach((feature) => {
        this.browserProxy_.setFeatureEnabledState(
            feature, true /* enabled */, this.authToken_);
      });
    }

    // If the features were enabled above, they no longer need to be tracked.
    // Likewise, if the features were not enabled above, the user did not enter
    // the correct password in the dialog, so the features should not actually
    // be enabled.
    this.featuresToBeEnabledOnceAuthenticated_ = [];

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

    if (!this.isAuthenticationRequiredForChange_(feature, enabled)) {
      this.browserProxy_.setFeatureEnabledState(
          feature, enabled, this.authToken_);
      return;
    }

    this.featuresToBeEnabledOnceAuthenticated_.push(feature);
    this.openPasswordPromptDialog_();
  },

  /**
   * @param {!settings.MultiDeviceFeature} feature The feature to change.
   * @param {boolean} enabled True if the user has requested to enable the
   *     feature; false if the user has requested to disable the feature.
   * @return {boolean} Whether authentication is required to change the
   *     feature's state.
   * @private
   */
  isAuthenticationRequiredForChange_: function(feature, enabled) {
    // Disabling a feature never requires authentication.
    if (!enabled)
      return false;

    // Enabling SmartLock always requires authentication.
    if (feature == settings.MultiDeviceFeature.SMART_LOCK)
      return true;

    // Enabling any feature besides SmartLock and the Better Together suite does
    // not require authentication.
    if (feature != settings.MultiDeviceFeature.BETTER_TOGETHER_SUITE)
      return false;

    let smartLockState =
        this.getFeatureState(settings.MultiDeviceFeature.SMART_LOCK);

    // If the user is enabling the Better Together suite and this change would
    // result in SmartLock being implicitly enabled, authentication is required.
    // SmartLock is implicitly enabled if it is only currently not enabled due
    // to the suite being disabled or due to the SmartLock host device not
    // having a lock screen set.
    return smartLockState ==
        settings.MultiDeviceFeatureState.UNAVAILABLE_SUITE_DISABLED ||
        smartLockState ==
        settings.MultiDeviceFeatureState.UNAVAILABLE_INSUFFICIENT_SECURITY;
  },

  /** @private */
  onForgetDeviceRequested_: function() {
    this.browserProxy_.removeHostDevice();
    settings.navigateTo(settings.routes.MULTIDEVICE);
  },
});
