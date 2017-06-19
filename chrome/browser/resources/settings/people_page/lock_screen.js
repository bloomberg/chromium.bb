// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-lock-screen' allows the user to change how they unlock their
 * device.
 *
 * Example:
 *
 * <settings-lock-screen
 *   prefs="{{prefs}}">
 * </settings-lock-screen>
 */

Polymer({
  is: 'settings-lock-screen',

  behaviors: [
    I18nBehavior,
    LockStateBehavior,
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {type: Object},

    /**
     * setModes_ is a partially applied function that stores the previously
     * entered password. It's defined only when the user has already entered a
     * valid password.
     *
     * @type {Object|undefined}
     * @private
     */
    setModes_: {
      type: Object,
      observer: 'onSetModesChanged_',
    },

    /**
     * writeUma_ is a function that handles writing uma stats. It may be
     * overridden for tests.
     *
     * @type {Function}
     * @private
     */
    writeUma_: {
      type: Object,
      value: function() {
        return settings.recordLockScreenProgress;
      },
    },

    /**
     * True if quick unlock settings should be displayed on this machine.
     * @private
     */
    quickUnlockEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('quickUnlockEnabled');
      },
      readOnly: true,
    },

    /**
     * True if fingerprint unlock settings should be displayed on this machine.
     * @private
     */
    fingerprintUnlockEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('fingerprintUnlockEnabled');
      },
      readOnly: true,
    },

    /** @private */
    numFingerprints_: {
      type: Number,
      value: 0,
    },

    /**
     * True if Easy Unlock is allowed on this machine.
     */
    easyUnlockAllowed_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('easyUnlockAllowed');
      },
      readOnly: true,
    },

    /**
     * True if Easy Unlock is enabled.
     */
    easyUnlockEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('easyUnlockEnabled');
      },
    },

    /**
     * True if Easy Unlock's proximity detection feature is allowed.
     */
    easyUnlockProximityDetectionAllowed_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('easyUnlockAllowed') &&
            loadTimeData.getBoolean('easyUnlockProximityDetectionAllowed');
      },
      readOnly: true,
    },

    /** @private */
    showEasyUnlockTurnOffDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showPasswordPromptDialog_: Boolean,

    /** @private */
    showSetupPinDialog_: Boolean,
  },

  /** @private {?settings.EasyUnlockBrowserProxy} */
  easyUnlockBrowserProxy_: null,

  /** @private {?settings.FingerprintBrowserProxy} */
  fingerprintBrowserProxy_: null,

  /** selectedUnlockType is defined in LockStateBehavior. */
  observers: ['selectedUnlockTypeChanged_(selectedUnlockType)'],

  /** @override */
  attached: function() {
    if (this.shouldAskForPassword_(settings.getCurrentRoute()))
      this.openPasswordPromptDialog_();

    this.easyUnlockBrowserProxy_ =
        settings.EasyUnlockBrowserProxyImpl.getInstance();
    this.fingerprintBrowserProxy_ =
        settings.FingerprintBrowserProxyImpl.getInstance();

    if (this.easyUnlockAllowed_) {
      this.addWebUIListener(
          'easy-unlock-enabled-status',
          this.handleEasyUnlockEnabledStatusChanged_.bind(this));
      this.easyUnlockBrowserProxy_.getEnabledStatus().then(
          this.handleEasyUnlockEnabledStatusChanged_.bind(this));
    }
  },

  /**
   * Overridden from settings.RouteObserverBehavior.
   * @param {!settings.Route} newRoute
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged: function(newRoute, oldRoute) {
    if (newRoute == settings.Route.LOCK_SCREEN &&
        this.fingerprintUnlockEnabled_ && this.fingerprintBrowserProxy_) {
      this.fingerprintBrowserProxy_.getNumFingerprints().then(
          function(numFingerprints) {
            this.numFingerprints_ = numFingerprints;
          }.bind(this));
    }

    if (this.shouldAskForPassword_(newRoute)) {
      this.openPasswordPromptDialog_();
    } else if (
        newRoute != settings.Route.FINGERPRINT &&
        oldRoute != settings.Route.FINGERPRINT) {
      // If the user navigated away from the lock screen settings page they will
      // have to re-enter their password. An exception is if they are navigating
      // to or from the fingerprint subpage.
      this.setModes_ = undefined;
    }
  },

  /**
   * Called when the unlock type has changed.
   * @param {!string} selected The current unlock type.
   * @private
   */
  selectedUnlockTypeChanged_: function(selected) {
    if (selected == LockScreenUnlockType.VALUE_PENDING)
      return;

    if (selected != LockScreenUnlockType.PIN_PASSWORD && this.setModes_) {
      this.setModes_.call(null, [], [], function(didSet) {
        assert(didSet, 'Failed to clear quick unlock modes');
      });
    }
  },

  /** @private */
  onSetModesChanged_: function() {
    if (this.shouldAskForPassword_(settings.getCurrentRoute())) {
      this.$.setupPin.close();
      this.openPasswordPromptDialog_();
    }
  },

  /** @private */
  openPasswordPromptDialog_: function() {
    this.showPasswordPromptDialog_ = true;
  },

  /** @private */
  onPasswordPromptDialogClose_: function() {
    this.showPasswordPromptDialog_ = false;
    if (!this.setModes_)
      settings.navigateToPreviousRoute();
    else
      cr.ui.focusWithoutInk(assert(this.$$('#unlockType')));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onConfigurePin_: function(e) {
    e.preventDefault();
    this.writeUma_(LockScreenProgress.CHOOSE_PIN_OR_PASSWORD);
    this.showSetupPinDialog_ = true;
  },

  /** @private */
  onSetupPinDialogClose_: function() {
    this.showSetupPinDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('#setupPinButton')));
  },

  /**
   * Returns true if the setup pin section should be shown.
   * @param {!string} selectedUnlockType The current unlock type. Used to let
   *     Polymer know about the dependency.
   * @private
   */
  showConfigurePinButton_: function(selectedUnlockType) {
    return selectedUnlockType === LockScreenUnlockType.PIN_PASSWORD;
  },

  /**
   * @param {boolean} hasPin
   * @private
   */
  getSetupPinText_: function(hasPin) {
    if (hasPin)
      return this.i18n('lockScreenChangePinButton');
    return this.i18n('lockScreenSetupPinButton');
  },

  /** @private */
  getDescriptionText_: function() {
    if (this.numFingerprints_ > 0) {
      return this.i18n(
          'lockScreenNumberFingerprints', this.numFingerprints_.toString());
    }

    return this.i18n('lockScreenEditFingerprintsDescription');
  },

  /** @private */
  onEditFingerprints_: function() {
    settings.navigateTo(settings.Route.FINGERPRINT);
  },

  /**
   * @param {!settings.Route} route
   * @return {boolean} Whether the password dialog should be shown.
   * @private
   */
  shouldAskForPassword_: function(route) {
    return route == settings.Route.LOCK_SCREEN && !this.setModes_;
  },

  /**
   * Handler for when the Easy Unlock enabled status has changed.
   * @private
   */
  handleEasyUnlockEnabledStatusChanged_: function(easyUnlockEnabled) {
    this.easyUnlockEnabled_ = easyUnlockEnabled;
    this.showEasyUnlockTurnOffDialog_ =
        easyUnlockEnabled && this.showEasyUnlockTurnOffDialog_;
  },

  /** @private */
  onEasyUnlockSetupTap_: function() {
    this.easyUnlockBrowserProxy_.startTurnOnFlow();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onEasyUnlockTurnOffTap_: function(e) {
    // Prevent the end of the tap event from focusing what is underneath the
    // button.
    e.preventDefault();
    this.showEasyUnlockTurnOffDialog_ = true;
  },

  /** @private */
  onEasyUnlockTurnOffDialogClose_: function() {
    this.showEasyUnlockTurnOffDialog_ = false;

    // Restores focus on close to either the turn-off or set-up button,
    // whichever is being displayed.
    cr.ui.focusWithoutInk(assert(this.$$('.secondary-button')));
  },

  /**
   * @param {boolean} enabled
   * @param {!string} enabledStr
   * @param {!string} disabledStr
   * @private
   */
  getEasyUnlockDescription_: function(enabled, enabledStr, disabledStr) {
    return enabled ? enabledStr : disabledStr;
  },

  /**
   * @param {boolean} easyUnlockEnabled
   * @param {boolean} proximityDetectionAllowed
   * @private
   */
  getShowEasyUnlockToggle_: function(
      easyUnlockEnabled, proximityDetectionAllowed) {
    return easyUnlockEnabled && proximityDetectionAllowed;
  },
});
