// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /**
   * All possible contentSettingsTypes that we currently support configuring in
   * the UI. Both top-level categories and content settings that represent
   * individual permissions under Site Details should appear here. This is a
   * subset of the constants found under content_setttings_types.h and the
   * values should be kept in sync.
   * @enum {number}
   */
  var ContentSettingsTypes = {
    COOKIES: 0,
    IMAGES: 1,
    JAVASCRIPT: 2,
    POPUPS: 4,
    GEOLOCATION: 5,
    NOTIFICATIONS: 6,
    FULLSCREEN: 8,
    MIC: 12,
    CAMERA: 13,
  };

  /**
   * Contains the possible string values for a given contentSettingsType.
   * @enum {string}
   */
  var PermissionValues = {
    DEFAULT: 'default',
    ALLOW: 'allow',
    BLOCK: 'block',
    ASK: 'ask',
  };

  /**
   * A category value to use for the All Sites list.
   * @const {number}
   */
  var ALL_SITES = -1;

  /**
   * An invalid subtype value.
   * @const {string}
   */
  var INVALID_CATEGORY_SUBTYPE = '';

  return {
    ContentSettingsTypes: ContentSettingsTypes,
    PermissionValues: PermissionValues,
    ALL_SITES: ALL_SITES,
    INVALID_CATEGORY_SUBTYPE: INVALID_CATEGORY_SUBTYPE,
  };
});
