// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-quick-unlock-choose-method' allows the user to change how they
 * unlock their device. Note that setting up the unlock method is delegated
 * to other elements.
 *
 * Example:
 *
 * <settings-quick-unlock-choose-method
 *   set-modes="[[quickUnlockSetModes]]"
 *   prefs="{{prefs}}">
 * </settings-quick-unlock-choose-method>
 */

(function() {
'use strict';

/** @const */ var ENABLE_LOCK_SCREEN_PREF = 'settings.enable_screen_lock';

/** @enum {string} */
var QuickUnlockUnlockType = {
  VALUE_PENDING: 'value_pending',
  NONE: 'none',
  PASSWORD: 'password',
  PIN_PASSWORD: 'pin+password'
};

Polymer({
  is: 'settings-quick-unlock-choose-method',

  behaviors: [PrefsBehavior, QuickUnlockPasswordDetectBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The currently selected unlock type.
     * @type {!QuickUnlockUnlockType}
     * @private
     */
    selectedUnlockType_: {
      type: String,
      notify: true,
      value: QuickUnlockUnlockType.VALUE_PENDING,
      observer: 'selectedUnlockTypeChanged_'
    }
  },

  observers: ['onSetModesChanged_(setModes)'],

  /** @override */
  attached: function() {
    CrSettingsPrefs.initialized.then(this.updateUnlockType_.bind(this));

    this.boundOnPrefsChanged_ = function(prefs) {
      for (var i = 0; i < prefs.length; ++i) {
        if (prefs[i].key == ENABLE_LOCK_SCREEN_PREF)
          this.updateUnlockType_();
      }
    }.bind(this);
    this.boundOnActiveModesChanged_ = this.updateUnlockType_.bind(this);

    chrome.settingsPrivate.onPrefsChanged.addListener(
        this.boundOnPrefsChanged_);
    chrome.quickUnlockPrivate.onActiveModesChanged.addListener(
        this.boundOnActiveModesChanged_);

    if (settings.getCurrentRoute() == settings.Route.QUICK_UNLOCK_CHOOSE_METHOD)
      this.askForPasswordIfUnset();
  },

  /** @override */
  detached: function() {
    chrome.settingsPrivate.onPrefsChanged.removeListener(
        this.boundOnPrefsChanged_);
    chrome.quickUnlockPrivate.onActiveModesChanged.removeListener(
        this.boundOnActiveModesChanged_);
  },

  /** @protected */
  currentRouteChanged: function() {
    if (settings.getCurrentRoute() == settings.Route.QUICK_UNLOCK_CHOOSE_METHOD)
      this.askForPasswordIfUnset();
  },

  /** @private */
  onSetModesChanged_: function() {
    if (settings.getCurrentRoute() == settings.Route.QUICK_UNLOCK_CHOOSE_METHOD)
      this.askForPasswordIfUnset();
  },

  /**
   * Updates the selected unlock type radio group. This function will get called
   * after preferences are initialized, after the quick unlock mode has been
   * changed, and after the lockscreen preference has changed.
   *
   * @private
   */
  updateUnlockType_: function() {
    // The quickUnlockPrivate.onActiveModesChanged event could trigger this
    // function before CrSettingsPrefs is initialized if another settings page
    // changes the quick unlock state.
    if (!CrSettingsPrefs.isInitialized)
      return;

    if (!this.getPref(ENABLE_LOCK_SCREEN_PREF).value) {
      this.selectedUnlockType_ = QuickUnlockUnlockType.NONE;
      return;
    }

    chrome.quickUnlockPrivate.getActiveModes(function(modes) {
      if (modes.includes(chrome.quickUnlockPrivate.QuickUnlockMode.PIN)) {
        this.selectedUnlockType_ = QuickUnlockUnlockType.PIN_PASSWORD;
      } else if (this.selectedUnlockType_ !=
                 QuickUnlockUnlockType.PIN_PASSWORD) {
        // We don't want to clobber an existing PIN+PASSWORD state because the
        // user needs to configure that option before actually using it.
        //
        // Specifically, this check is needed because this function gets called
        // by the pref system after changing the unlock type from NONE to
        // PIN+PASSWORD. Without the conditional assignment, this function would
        // change the PIN+PASSWORD lock type to PASSWORD because the PIN hasn't
        // been configured yet.
        this.selectedUnlockType_ = QuickUnlockUnlockType.PASSWORD;
      }
    }.bind(this));
  },

  /**
   * Called when the unlock type has changed.
   * @param {!string} selected The current unlock type.
   * @param {?string} previous The old unlock type. undefined if not present.
   * @private
   */
  selectedUnlockTypeChanged_: function(selected, previous) {
    // This method gets invoked when setting the initial value from the existing
    // state. In that case, we don't need to bother updating the prefs.
    if (!previous)
      return;

    this.setPrefValue(ENABLE_LOCK_SCREEN_PREF,
                      selected != QuickUnlockUnlockType.NONE);
    if (selected != QuickUnlockUnlockType.PIN_PASSWORD && this.setModes) {
      this.setModes.call(null, [], [], function(didSet) {
        assert(didSet, 'Failed to clear quick unlock modes');
      });
    }
  },

  /**
   * Retruns true if the setup pin section should be shown.
   * @param {!string} selectedUnlockType The current unlock type. Used to let
   * polymer know about the dependency.
   * @private
   */
  showSetupPin_: function(selectedUnlockType) {
    return selectedUnlockType === QuickUnlockUnlockType.PIN_PASSWORD;
  },

  /** @private */
  onConfigurePin_: function() {
    settings.navigateTo(settings.Route.QUICK_UNLOCK_SETUP_PIN);
  },
});

})();
