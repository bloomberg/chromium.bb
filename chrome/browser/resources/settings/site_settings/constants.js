// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /**
   * The possible contentSettingsTypes (the ones we currently support
   * configuring in the UI). This is a subset of the constants found under
   * content_setttings_types.h and the values should be kept in sync.
   * TODO(finnur): When all categories have been implemented we can just
   * generate these constants from content_setttings_types.h.
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
   * Contains the possible values for a given contentSettingsType.
   * @enum {number}
   */
  var PermissionValues = {
    ALLOW: 1,
    BLOCK: 2,
    ASK: 3,
  };

  return {
    ContentSettingsTypes: ContentSettingsTypes,
    PermissionValues: PermissionValues,
  };
});
