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
 * Possible values of the system time-zone auto-detection policy. Must stay in
 * sync with AutomaticTimezoneDetectionType in chrome_device_policy.proto.
 * @enum {number}
 * @const
 */
settings.AutomaticTimezoneDetectionPolicy = {
  USERS_DECIDE: 0,
  DISABLED: 1,
  IP_ONLY: 2,
  SEND_WIFI_ACCESS_POINTS: 3,
};

Polymer({
  is: 'settings-date-time-page',

  behaviors: [PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * If the time zone is managed at the system level, the user cannot control
     * any time zone settings.
     * @private
     */
    systemTimeZoneManaged_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('systemTimeZoneManaged');
      },
    },

    /**
     * If the time zone auto-detection setting is managed at the system level,
     * the user cannot override the setting unless the policy is USERS_DECIDE.
     * @private
     */
    systemTimeZoneDetectionManaged_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean(
            'systemTimeZoneDetectionManaged');
      },
    },

    /**
     * Value of the system time zone auto-detection policy, or null if system
     * time zone auto-detection is not managed.
     * @private {?settings.AutomaticTimezoneDetectionPolicy}
     */
    systemTimeZoneDetectionPolicyValue_: {
      type: Number,
      value: function() {
        return loadTimeData.valueExists('systemTimeZoneDetectionPolicyValue') ?
            /** @type {settings.AutomaticTimezoneDetectionPolicy} */(
                loadTimeData.getInteger('systemTimeZoneDetectionPolicyValue')) :
            null;
      },
    },

    /**
     * Hides the time zone auto-detection feature when the
     * --disable-timezone-tracking-option flag is set.
     * @private
     */
    hideTimeZoneDetection_: {
      type: Boolean,
      value: function() {
        return loadTimeData.valueExists('hideTimeZoneDetection') &&
               loadTimeData.getBoolean('hideTimeZoneDetection');
      },
      readOnly: true,
    },
  },

  observers: [
    // TODO(michaelpg): Implement a BrowserProxy to listen for policy changes.
    'updateTimeZoneDetectionCheckbox_(' +
        'prefs.settings.resolve_timezone_by_geolocation.value,' +
        'systemTimeZoneManaged_,' +
        'systemTimeZoneDetectionManaged_,' +
        'systemTimeZoneDetectionPolicyValue_)',
  ],

  /**
   * Processes all the time zone preferences and policy interactions in one
   * observer function instead of complicating the HTML with computed bindings.
   * @param {boolean} userPrefValue
   * @param {boolean} systemTimeZoneManaged
   * @param {boolean} systemTimeZoneDetectionManaged
   * @param {?settings.AutomaticTimezoneDetectionPolicy}
   *     systemTimeZoneDetectionPolicyValue
   */
  updateTimeZoneDetectionCheckbox_(userPrefValue,
                                   systemTimeZoneManaged,
                                   systemTimeZoneDetectionManaged,
                                   systemTimeZoneDetectionPolicyValue) {
    // Do nothing if the feature is disabled by a flag.
    if (this.hideTimeZoneDetection_)
      return;

    var checkbox = this.$.timeZoneDetectionCheckbox;

    // Time zone auto-detection is disabled when the time zone is managed.
    if (systemTimeZoneManaged) {
      checkbox.disabled = true;
      checkbox.checked = false;
      return;
    }

    // The time zone auto-detection policy may force-disable auto-detection.
    if (systemTimeZoneDetectionManaged &&
        systemTimeZoneDetectionPolicyValue !=
        settings.AutomaticTimezoneDetectionPolicy.USERS_DECIDE) {
      checkbox.disabled = true;
      checkbox.checked =
          systemTimeZoneDetectionPolicyValue !=
          settings.AutomaticTimezoneDetectionPolicy.DISABLED;
      return;
    }

    // If there is no policy, or the policy is USERS_DECIDE, the pref is used.
    checkbox.disabled = false;
    checkbox.checked = userPrefValue;
  },

  /** @param {!Event} e */
  onTimeZoneDetectionCheckboxChange_: function(e) {
    this.setPrefValue(
        'settings.resolve_timezone_by_geolocation', e.target.checked);
  },
});
