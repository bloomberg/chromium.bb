// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-date-time-page' is the settings page containing date and time
 * settings.
 */

Polymer({
  is: 'settings-date-time-page',

  behaviors: [I18nBehavior, PrefsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * The effective policy restriction on time zone automatic detection.
     * @private {settings.TimeZoneAutoDetectPolicyRestriction}
     */
    timeZoneAutoDetectPolicyRestriction_: {
      type: Number,
      value: function() {
        if (!loadTimeData.valueExists('timeZoneAutoDetectValueFromPolicy'))
          return settings.TimeZoneAutoDetectPolicyRestriction.NONE;
        return loadTimeData.getBoolean('timeZoneAutoDetectValueFromPolicy') ?
            settings.TimeZoneAutoDetectPolicyRestriction.FORCED_ON :
            settings.TimeZoneAutoDetectPolicyRestriction.FORCED_OFF;
      },
    },

    /**
     * Whether a policy controls the time zone auto-detect setting.
     * @private
     */
    hasTimeZoneAutoDetectPolicyRestriction_: {
      type: Boolean,
      computed: 'computeHasTimeZoneAutoDetectPolicy_(' +
          'timeZoneAutoDetectPolicyRestriction_)',
    },

    /**
     * The effective time zone auto-detect enabled/disabled status.
     * @private
     */
    timeZoneAutoDetect_: {
      type: Boolean,
      computed: 'computeTimeZoneAutoDetect_(' +
          'timeZoneAutoDetectPolicyRestriction_,' +
          'prefs.settings.resolve_timezone_by_geolocation_method.value)',
    },

    /**
     * The effective time zone auto-detect method.
     * @private {settings.TimeZoneAutoDetectMethod}
     */
    timeZoneAutoDetectMethod_: {
      type: Number,
      computed: 'computeTimeZoneAutoDetectMethod_(' +
          'hasTimeZoneAutoDetectPolicyRestriction_,' +
          'prefs.settings.resolve_timezone_by_geolocation_method.value)',
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

    /**
     * This is used to get current time zone display name from
     * <timezone-selector> via bi-directional binding.
     */
    activeTimeZoneDisplayName: {
      type: String,
      value: loadTimeData.getString('timeZoneName'),
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        var map = new Map();
        if (settings.routes.DATETIME_TIMEZONE_SUBPAGE)
          map.set(
              settings.routes.DATETIME_TIMEZONE_SUBPAGE.path,
              '#timeZoneSettingsTrigger .subpage-arrow');
        return map;
      },
    },
  },

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'time-zone-auto-detect-policy',
        this.onTimeZoneAutoDetectPolicyChanged_.bind(this));
    this.addWebUIListener(
        'can-set-date-time-changed', this.onCanSetDateTimeChanged_.bind(this));

    chrome.send('dateTimePageReady');
  },

  /**
   * @param {boolean} managed Whether the auto-detect setting is controlled.
   * @param {boolean} valueFromPolicy The value of the auto-detect setting
   *     forced by policy.
   * @private
   */
  onTimeZoneAutoDetectPolicyChanged_: function(managed, valueFromPolicy) {
    if (managed) {
      this.timeZoneAutoDetectPolicyRestriction_ = valueFromPolicy ?
          settings.TimeZoneAutoDetectPolicyRestriction.FORCED_ON :
          settings.TimeZoneAutoDetectPolicyRestriction.FORCED_OFF;
    } else {
      this.timeZoneAutoDetectPolicyRestriction_ =
          settings.TimeZoneAutoDetectPolicyRestriction.NONE;
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
   * @param {settings.TimeZoneAutoDetectPolicyRestriction} policyValue
   * @return {boolean}
   * @private
   */
  computeHasTimeZoneAutoDetectPolicy_: function(policyValue) {
    return policyValue != settings.TimeZoneAutoDetectPolicyRestriction.NONE;
  },

  /**
   * @param {settings.TimeZoneAutoDetectPolicyRestriction} policyValue
   * @param {settings.TimeZoneAutoDetectMethod} prefValue
   *     prefs.settings.resolve_timezone_by_geolocation_method.value
   * @return {boolean} Whether time zone auto-detect is enabled.
   * @private
   */
  computeTimeZoneAutoDetect_: function(policyValue, prefValue) {
    switch (policyValue) {
      case settings.TimeZoneAutoDetectPolicyRestriction.NONE:
        return prefValue != settings.TimeZoneAutoDetectMethod.DISABLED;
      case settings.TimeZoneAutoDetectPolicyRestriction.FORCED_ON:
        return true;
      case settings.TimeZoneAutoDetectPolicyRestriction.FORCED_OFF:
        return false;
      default:
        console.error('Unknown policy value "' + policyValue + '".');
        return false;
    }
  },

  /**
   * Computes effective time zone detection method.
   * @param {Boolean} hasTimeZoneAutoDetectPolicyRestriction
   *     this.hasTimeZoneAutoDetectPolicyRestriction_
   * @param {settings.TimeZoneAutoDetectMethod} prefResolveValue
   *     prefs.settings.resolve_timezone_by_geolocation_method.value
   * @return {settings.TimeZoneAutoDetectMethod}
   * @private
   */
  computeTimeZoneAutoDetectMethod_: function(
      hasTimeZoneAutoDetectPolicyRestriction, prefResolveValue) {
    if (hasTimeZoneAutoDetectPolicyRestriction) {
      // timeZoneAutoDetectPolicyRestriction_ actually depends on several time
      // policies and chrome flags. So we ignore real policy value if it is
      // disabled.
      if (this.timeZoneAutoDetectPolicyRestriction_ ==
          settings.TimeZoneAutoDetectPolicyRestriction.FORCED_OFF) {
        return settings.TimeZoneAutoDetectMethod.DISABLED;
      }

      var policyValue = /** @type{settings.SystemTimezoneProto} */ (
          this.getPref('settings.resolve_device_timezone_by_geolocation_policy')
              .value);

      switch (policyValue) {
        case settings.SystemTimezoneProto.USERS_DECIDE:
          console.error('Unexpected policy value "' + policyValue + '".');
          return settings.TimeZoneAutoDetectMethod.DISABLED;
        case settings.SystemTimezoneProto.DISABLED:
          return settings.TimeZoneAutoDetectMethod.DISABLED;
        case settings.SystemTimezoneProto.IP_ONLY:
          return settings.TimeZoneAutoDetectMethod.IP_ONLY;
        case settings.SystemTimezoneProto.SEND_WIFI_ACCESS_POINTS:
          return settings.TimeZoneAutoDetectMethod.SEND_WIFI_ACCESS_POINTS;
        case settings.SystemTimezoneProto.SEND_ALL_LOCATION_INFO:
          return settings.TimeZoneAutoDetectMethod.SEND_ALL_LOCATION_INFO;
        default:
          return settings.TimeZoneAutoDetectMethod.DISABLED;
      }
    }
    return prefResolveValue;
  },

  /**
   * Returns display name of the given time zone detection method.
   * @param {settings.TimeZoneAutoDetectMethod} method
   *     this.timeZoneAutoDetectMethod_ value.
   * @return {string}
   * @private
   */
  getTimeZoneAutoDetectMethodDisplayName_: function(method) {
    var id = ([
      'setTimeZoneAutomaticallyDisabled',
      'setTimeZoneAutomaticallyIpOnlyDefault',
      'setTimeZoneAutomaticallyWithWiFiAccessPointsData',
      'setTimeZoneAutomaticallyWithAllLocationInfo'
    ])[method];
    if (id)
      return this.i18n(id);

    return '';
  },

  onTimeZoneSettings_: function() {
    // TODO(alemate): revise this once UI mocks are finished.
    settings.navigateTo(settings.routes.DATETIME_TIMEZONE_SUBPAGE);
  },
});
