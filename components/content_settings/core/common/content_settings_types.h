// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_TYPES_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_TYPES_H_

#include <cstddef>

#include "build/build_config.h"

// A particular type of content to care about.  We give the user various types
// of controls over each of these.
// When adding/removing values from this enum, be sure to update
// ContentSettingsTypeHistogram and ContentSettingTypeToHistogramValue in
// content_settings.cc as well.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser
enum ContentSettingsType {
  // "DEFAULT" is only used as an argument to the Content Settings Window
  // opener; there it means "whatever was last shown".
  CONTENT_SETTINGS_TYPE_DEFAULT = -1,
  CONTENT_SETTINGS_TYPE_COOKIES = 0,
  CONTENT_SETTINGS_TYPE_IMAGES,
  CONTENT_SETTINGS_TYPE_JAVASCRIPT,
  CONTENT_SETTINGS_TYPE_PLUGINS,
  CONTENT_SETTINGS_TYPE_POPUPS,
  CONTENT_SETTINGS_TYPE_GEOLOCATION,
  CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
  CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
  CONTENT_SETTINGS_TYPE_MIXEDSCRIPT,
  CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
  CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
  CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS,
  CONTENT_SETTINGS_TYPE_PPAPI_BROKER,
  CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
  CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
  CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS,
  CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
  CONTENT_SETTINGS_TYPE_APP_BANNER,
  CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT,
  CONTENT_SETTINGS_TYPE_DURABLE_STORAGE,
  CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA,
  CONTENT_SETTINGS_TYPE_BLUETOOTH_GUARD,
  CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC,
  CONTENT_SETTINGS_TYPE_AUTOPLAY,
  // TODO(raymes): Deprecated. See crbug.com/681709. Remove after M60.
  CONTENT_SETTINGS_TYPE_PROMPT_NO_DECISION_COUNT,
  CONTENT_SETTINGS_TYPE_IMPORTANT_SITE_INFO,
  CONTENT_SETTINGS_TYPE_PERMISSION_AUTOBLOCKER_DATA,
  CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER,

  // Website setting which stores metadata for the subresource filter to aid in
  // decisions for whether or not to show the UI.
  CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER_DATA,

  // This is special-cased in the permissions layer to always allow, and as
  // such doesn't have associated prefs data.
  CONTENT_SETTINGS_TYPE_MIDI,

  // This is only here temporarily and will be removed when we further unify
  // it with notifications, see crbug.com/563297. No prefs data is stored for
  // this content type, we instead share values with NOTIFICATIONS.
  CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,

  // This content setting type is for caching password protection service's
  // verdicts of each origin.
  CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION,

  // WARNING: This enum is going to be removed soon. Do not depend on NUM_TYPES.
  CONTENT_SETTINGS_NUM_TYPES_DO_NOT_USE,
};

struct ContentSettingsTypeHash {
  std::size_t operator()(ContentSettingsType type) const {
    return static_cast<std::size_t>(type);
  }
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_TYPES_H_
