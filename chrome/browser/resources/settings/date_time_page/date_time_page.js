// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-date-time-page' is the settings page containing date and time
 * settings.
 */

cr.exportPath('settings');

/**
 * Describes the status of the auto-detect policy.
 * @enum {number}
 */
settings.TimeZoneAutoDetectPolicy = {
  NONE: 0,
  FORCED_ON: 1,
  FORCED_OFF: 2,
};

/**
 * Describes values of prefs.settings.resolve_timezone_by_geolocation_method.
 * Must be kept in sync with TimeZoneResolverManager::TimeZoneResolveMethod
 * enum.
 * @enum {number}
 */
settings.TimeZoneAutoDetectMethod = {
  DISABLED: 0,
  IP_ONLY: 1,
  SEND_WIFI_ACCESS_POINTS: 2,
  SEND_ALL_LOCATION_INFO: 3
};

Polymer({
  is: 'settings-date-time-page',

  behaviors: [PrefsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * The time zone auto-detect policy.
     * @private {settings.TimeZoneAutoDetectPolicy}
     */
    timeZoneAutoDetectPolicy_: {
      type: Boolean,
      value: function() {
        if (!loadTimeData.valueExists('timeZoneAutoDetectValueFromPolicy'))
          return settings.TimeZoneAutoDetectPolicy.NONE;
        return loadTimeData.getBoolean('timeZoneAutoDetectValueFromPolicy') ?
            settings.TimeZoneAutoDetectPolicy.FORCED_ON :
            settings.TimeZoneAutoDetectPolicy.FORCED_OFF;
      },
    },

    /**
     * Whether a policy controls the time zone auto-detect setting.
     * @private
     */
    hasTimeZoneAutoDetectPolicy_: {
      type: Boolean,
      computed:
          'computeHasTimeZoneAutoDetectPolicy_(timeZoneAutoDetectPolicy_)',
    },

    /**
     * The effective time zone auto-detect setting.
     * @private
     */
    timeZoneAutoDetect_: {
      type: Boolean,
      computed: 'computeTimeZoneAutoDetect_(' +
          'timeZoneAutoDetectPolicy_,' +
          'prefs.settings.resolve_timezone_by_geolocation_method.value)',
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

    /**
     * Whether date and time are settable. Normally the date and time are forced
     * by network time, so default to false to initially hide the button.
     * @private
     */
    canSetDateTime_: {
      type: Boolean,
      value: false,
    },
  },

  observers: [
    'maybeGetTimeZoneListPerUser_(' +
        'prefs.settings.timezone.value, timeZoneAutoDetect_)',
    'maybeGetTimeZoneListPerSystem_(' +
        'prefs.cros.system.timezone.value, timeZoneAutoDetect_)',
  ],

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'time-zone-auto-detect-policy',
        this.onTimeZoneAutoDetectPolicyChanged_.bind(this));
    this.addWebUIListener(
        'can-set-date-time-changed', this.onCanSetDateTimeChanged_.bind(this));

    chrome.send('dateTimePageReady');
    this.maybeGetTimeZoneList_();
  },

  /**
   * @param {boolean} managed Whether the auto-detect setting is controlled.
   * @param {boolean} valueFromPolicy The value of the auto-detect setting
   *     forced by policy.
   * @private
   */
  onTimeZoneAutoDetectPolicyChanged_: function(managed, valueFromPolicy) {
    if (managed) {
      this.timeZoneAutoDetectPolicy_ = valueFromPolicy ?
          settings.TimeZoneAutoDetectPolicy.FORCED_ON :
          settings.TimeZoneAutoDetectPolicy.FORCED_OFF;
    } else {
      this.timeZoneAutoDetectPolicy_ = settings.TimeZoneAutoDetectPolicy.NONE;
    }
  },

  /**
   * @param {boolean} canSetDateTime Whether date and time are settable.
   * @private
   */
  onCanSetDateTimeChanged_: function(canSetDateTime) {
    this.canSetDateTime_ = canSetDateTime;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onTimeZoneAutoDetectChange_: function(e) {
    this.setPrefValue(
        'settings.resolve_timezone_by_geolocation_method',
        e.target.checked ? settings.TimeZoneAutoDetectMethod.IP_ONLY :
                           settings.TimeZoneAutoDetectMethod.DISABLED);
  },

  /** @private */
  onSetDateTimeTap_: function() {
    chrome.send('showSetDateTimeUI');
  },

  /**
   * @param {settings.TimeZoneAutoDetectPolicy} timeZoneAutoDetectPolicy
   * @return {boolean}
   * @private
   */
  computeHasTimeZoneAutoDetectPolicy_: function(timeZoneAutoDetectPolicy) {
    return timeZoneAutoDetectPolicy != settings.TimeZoneAutoDetectPolicy.NONE;
  },

  /**
   * @param {settings.TimeZoneAutoDetectPolicy} timeZoneAutoDetectPolicy
   * @param {settings.TimeZoneAutoDetectMethod} prefValue
   *     prefs.settings.resolve_timezone_by_geolocation_method.value
   * @return {boolean} Whether time zone auto-detect is enabled.
   * @private
   */
  computeTimeZoneAutoDetect_: function(timeZoneAutoDetectPolicy, prefValue) {
    switch (timeZoneAutoDetectPolicy) {
      case settings.TimeZoneAutoDetectPolicy.NONE:
        return prefValue != settings.TimeZoneAutoDetectMethod.DISABLED;
      case settings.TimeZoneAutoDetectPolicy.FORCED_ON:
        return true;
      case settings.TimeZoneAutoDetectPolicy.FORCED_OFF:
        return false;
      default:
        assertNotReached();
    }
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
    if (this.timeZoneAutoDetect_) {
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
  },

  /**
   * Computes visibility of user timezone preference.
   * @param {?chrome.settingsPrivate.PrefObject} prefUserTimezone
   *     pref.settings.timezone
   * @param {settings.TimeZoneAutoDetectMethod} prefResolveValue
   *     prefs.settings.resolve_timezone_by_geolocation_method.value
   * @private
   */
  isUserTimeZoneSelectorHidden_: function(prefUserTimezone, prefResolveValue) {
    return (prefUserTimezone && prefUserTimezone.controlledBy != null) ||
        prefResolveValue != settings.TimeZoneAutoDetectMethod.DISABLED;
  },
});
