// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'timezone-selector' is the time zone selector dropdown.
 */
(function() {
'use strict';

Polymer({
  is: 'timezone-selector',

  behaviors: [I18nBehavior, PrefsBehavior],

  properties: {
    /**
     * If time zone auto detectoin is enabled.
     */
    timeZoneAutoDetect: Boolean,

    /**
     * This stores active time zone display name to be used in other UI
     * via bi-directional binding.
     */
    activeTimeZoneDisplayName: {
      type: String,
      notify: true,
    },

    /**
     * Initialized with the current time zone so the menu displays the
     * correct value. The full option list is fetched lazily if necessary by
     * maybeGetTimeZoneList_.
     * @private {!DropdownMenuOptionList}
     */
    timeZoneList_: {
      type: Array,
      value: function() {
        return [{
          name: loadTimeData.getString('timeZoneName'),
          value: loadTimeData.getString('timeZoneID'),
        }];
      },
    },
  },

  observers: [
    'maybeGetTimeZoneListPerUser_(' +
        'prefs.settings.timezone.value, timeZoneAutoDetect)',
    'maybeGetTimeZoneListPerSystem_(' +
        'prefs.cros.system.timezone.value, timeZoneAutoDetect)',
    'updateActiveTimeZoneName_(prefs.cros.system.timezone.value)',
  ],

  /** @override */
  attached: function() {
    this.maybeGetTimeZoneList_();
  },

  /**
   * Fetches the list of time zones if necessary.
   * @param {boolean=} perUserTimeZoneMode Expected value of per-user time zone.
   * @private
   */
  maybeGetTimeZoneList_: function(perUserTimeZoneMode) {
    if (typeof(perUserTimeZoneMode) !== 'undefined') {
      /* This method is called as observer. Skip if if current mode does not
       * match expected.
       */
      if (perUserTimeZoneMode !=
          this.getPref('cros.flags.per_user_timezone_enabled').value) {
        return;
      }
    }
    // Only fetch the list once.
    if (this.timeZoneList_.length > 1 || !CrSettingsPrefs.isInitialized)
      return;

    // If auto-detect is enabled, we only need the current time zone.
    if (this.timeZoneAutoDetect) {
      var isPerUserTimezone =
          this.getPref('cros.flags.per_user_timezone_enabled').value;
      if (this.timeZoneList_[0].value ==
          (isPerUserTimezone ? this.getPref('settings.timezone').value :
                               this.getPref('cros.system.timezone').value)) {
        return;
      }
    }

    cr.sendWithPromise('getTimeZones').then(this.setTimeZoneList_.bind(this));
  },

  /**
   * Prefs observer for Per-user time zone enabled mode.
   * @private
   */
  maybeGetTimeZoneListPerUser_: function() {
    this.maybeGetTimeZoneList_(true);
  },

  /**
   * Prefs observer for Per-user time zone disabled mode.
   * @private
   */
  maybeGetTimeZoneListPerSystem_: function() {
    this.maybeGetTimeZoneList_(false);
  },

  /**
   * Converts the C++ response into an array of menu options.
   * @param {!Array<!Array<string>>} timeZones C++ time zones response.
   * @private
   */
  setTimeZoneList_: function(timeZones) {
    this.timeZoneList_ = timeZones.map(function(timeZonePair) {
      return {
        name: timeZonePair[1],
        value: timeZonePair[0],
      };
    });
    this.updateActiveTimeZoneName_(
        /** @type {!String} */ (this.getPref('cros.system.timezone').value));
  },

  /**
   * Updates active time zone display name when changed.
   * @param {!String} activeTimeZoneId value of cros.system.timezone preference.
   * @private
   */
  updateActiveTimeZoneName_: function(activeTimeZoneId) {
    var activeTimeZone = this.timeZoneList_.find(
        (timeZone) => timeZone.value == activeTimeZoneId);
    if (activeTimeZone)
      this.activeTimeZoneDisplayName = activeTimeZone.name;
  },


  /**
   * Computes visibility of user timezone preference.
   * @param {?chrome.settingsPrivate.PrefObject} prefUserTimezone
   *     pref.settings.timezone
   * @param {settings.TimeZoneAutoDetectMethod} prefResolveValue
   *     prefs.settings.resolve_timezone_by_geolocation_method.value
   * @return {boolean}
   * @private
   */
  isUserTimeZoneSelectorHidden_: function(prefUserTimezone, prefResolveValue) {
    return (prefUserTimezone && prefUserTimezone.controlledBy != null) ||
        prefResolveValue != settings.TimeZoneAutoDetectMethod.DISABLED;
  },
});
})();
