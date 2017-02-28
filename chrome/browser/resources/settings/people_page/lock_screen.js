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

  behaviors: [I18nBehavior, LockStateBehavior, settings.RouteObserverBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object
    },

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
      observer: 'onSetModesChanged_'
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
      value: function() { return settings.recordLockScreenProgress; }
    },

    /**
     * True if pin unlock settings should be displayed on this machine.
     * @private
     */
    pinUnlockEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('pinUnlockEnabled');
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
  },

  /** @private {?settings.FingerprintBrowserProxy} */
  browserProxy_: null,

  /** selectedUnlockType is defined in LockStateBehavior. */
  observers: ['selectedUnlockTypeChanged_(selectedUnlockType)'],

  /** @override */
  attached: function() {
    if (this.shouldAskForPassword_(settings.getCurrentRoute()))
      this.$.passwordPrompt.open();
    this.browserProxy_ = settings.FingerprintBrowserProxyImpl.getInstance();
  },

  /**
   * Overridden from settings.RouteObserverBehavior.
   * @param {!settings.Route} newRoute
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged: function(newRoute, oldRoute) {
    if (newRoute == settings.Route.LOCK_SCREEN &&
        this.fingerprintUnlockEnabled_ &&
        this.browserProxy_) {
      this.browserProxy_.getNumFingerprints().then(
          function(numFingerprints) {
            this.numFingerprints_ = numFingerprints;
          }.bind(this));
    }

    if (this.shouldAskForPassword_(newRoute)) {
      this.$.passwordPrompt.open();
    } else if (newRoute != settings.Route.FINGERPRINT &&
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
      this.$.passwordPrompt.open();
    }
  },

  /** @private */
  onPasswordClosed_: function() {
    if (!this.setModes_)
      settings.navigateTo(settings.Route.PEOPLE);
  },

  /** @private */
  onPinSetupDone_: function() {
    this.$.setupPin.close();
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

  /** @private */
  getSetupPinText_: function() {
    if (this.hasPin)
      return this.i18n('lockScreenChangePinButton');
    return this.i18n('lockScreenSetupPinButton');
  },

  /** @private */
  getDescriptionText_: function() {
    if (this.numFingerprints_ > 0) {
      return this.i18n('lockScreenNumberFingerprints',
          this.numFingerprints_.toString());
    }

    return this.i18n('lockScreenEditFingerprintsDescription');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onConfigurePin_: function(e) {
    e.preventDefault();
    this.$.setupPin.open();
    this.writeUma_(LockScreenProgress.CHOOSE_PIN_OR_PASSWORD);
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
});
