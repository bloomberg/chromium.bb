// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CONTENT_PERMISSION_TYPES_H_
#define CHROME_COMMON_CONTENT_PERMISSION_TYPES_H_

// Indicates different permission levels that can be assigned for a given
// resource.
enum ContentPermissionType {
  CONTENT_PERMISSION_FIRST_TYPE = 0,
  CONTENT_PERMISSION_TYPE_BLOCK = CONTENT_PERMISSION_FIRST_TYPE,
  CONTENT_PERMISSION_TYPE_ALLOW,
  CONTENT_PERMISSION_TYPE_ASK,
  CONTENT_PERMISSION_TYPE_DEFAULT,
  CONTENT_PERMISSION_NUM_TYPES
};

// A particular type of content to care about.  We give the user various types
// of controls over each of these.
enum ContentSettingsType {
  // "DEFAULT" is only used as an argument to the Content Settings Window
  // opener; there it means "whatever was last shown".
  CONTENT_SETTINGS_TYPE_DEFAULT = -1,
  CONTENT_SETTINGS_FIRST_TYPE = 0,
  CONTENT_SETTINGS_TYPE_COOKIES = CONTENT_SETTINGS_FIRST_TYPE,
  CONTENT_SETTINGS_TYPE_IMAGES,
  CONTENT_SETTINGS_TYPE_JAVASCRIPT,
  CONTENT_SETTINGS_TYPE_PLUGINS,
  CONTENT_SETTINGS_TYPE_POPUPS,
  CONTENT_SETTINGS_NUM_TYPES
};

// Aggregates the permissions for the different content types.
struct ContentPermissions {
  ContentPermissionType permissions[CONTENT_SETTINGS_NUM_TYPES];

  ContentPermissions() {
    for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
      permissions[i] = CONTENT_PERMISSION_TYPE_ALLOW;
  }

  // Converts the struct into an integer used for storing in preferences and
  // sending over IPC.
  static int ToInteger(const ContentPermissions& permissions) {
    int result = 0;
    for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
      result = ((result << CONTENT_PERMISSION_NUM_TYPES) +
                (1 << permissions.permissions[i]));
    return result;
  }

  // Converts an integer as stored in preferences or sent over IPC to
  // ContentPermissions.
  static ContentPermissions FromInteger(int settings) {
    ContentPermissions result;
    for (int i = CONTENT_SETTINGS_NUM_TYPES - 1; i >= 0; --i) {
      int bitmask = (settings & ((1 << CONTENT_PERMISSION_NUM_TYPES) - 1));
      settings >>= CONTENT_PERMISSION_NUM_TYPES;
      int value = 0;
      while (bitmask != 1) {
        value++;
        bitmask >>= 1;
      }
      result.permissions[i] = static_cast<ContentPermissionType>(value);
    }
    return result;
  }
};

#endif  // CHROME_COMMON_CONTENT_PERMISSION_TYPES_H_
