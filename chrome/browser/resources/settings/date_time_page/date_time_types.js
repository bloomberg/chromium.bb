// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This defines some types for settings-date-time-page.
 */

cr.exportPath('settings');

/**
 * Describes the effective policy restriction on time zone automatic detection.
 * @enum {number}
 */
settings.TimeZoneAutoDetectPolicyRestriction = {
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

/**
 * Describes values of prefs.settings.
 * resolve_device_timezone_by_geolocation_policy
 * Must be kept in sync with enterprise_management::SystemTimezoneProto
 * enum.
 * @enum {number}
 */
settings.SystemTimezoneProto = {
  USERS_DECIDE: 0,
  DISABLED: 1,
  IP_ONLY: 2,
  SEND_WIFI_ACCESS_POINTS: 3,
  SEND_ALL_LOCATION_INFO: 4,
};
