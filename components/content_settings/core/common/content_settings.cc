// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings.h"

#include "base/logging.h"

ContentSetting IntToContentSetting(int content_setting) {
  return ((content_setting < 0) ||
          (content_setting >= CONTENT_SETTING_NUM_SETTINGS)) ?
      CONTENT_SETTING_DEFAULT : static_cast<ContentSetting>(content_setting);
}

ContentSettingsTypeHistogram ContentSettingTypeToHistogramValue(
    ContentSettingsType content_setting) {
  switch (content_setting) {
    case CONTENT_SETTINGS_TYPE_DEFAULT:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_INVALID;
    case CONTENT_SETTINGS_TYPE_COOKIES:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_COOKIES;
    case CONTENT_SETTINGS_TYPE_IMAGES:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_IMAGES;
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_JAVASCRIPT;
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_PLUGINS;
    case CONTENT_SETTINGS_TYPE_POPUPS:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_POPUPS;
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_GEOLOCATION;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_NOTIFICATIONS;
    case CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_AUTO_SELECT_CERTIFICATE;
    case CONTENT_SETTINGS_TYPE_FULLSCREEN:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_FULLSCREEN;
    case CONTENT_SETTINGS_TYPE_MOUSELOCK:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_MOUSELOCK;
    case CONTENT_SETTINGS_TYPE_MIXEDSCRIPT:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_MIXEDSCRIPT;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_MEDIASTREAM;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_MEDIASTREAM_MIC;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_MEDIASTREAM_CAMERA;
    case CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_PROTOCOL_HANDLERS;
    case CONTENT_SETTINGS_TYPE_PPAPI_BROKER:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_PPAPI_BROKER;
    case CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_AUTOMATIC_DOWNLOADS;
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_MIDI_SYSEX;
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_PUSH_MESSAGING;
    case CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_SSL_CERT_DECISIONS;
#if defined(OS_WIN)
    case CONTENT_SETTINGS_TYPE_METRO_SWITCH_TO_DESKTOP:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_METRO_SWITCH_TO_DESKTOP;
#elif defined(OS_ANDROID) || defined(OS_CHROMEOS)
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_PROTECTED_MEDIA_IDENTIFIER;
#endif
    case CONTENT_SETTINGS_TYPE_APP_BANNER:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_APP_BANNER;
    case CONTENT_SETTINGS_NUM_TYPES:
      return CONTENT_SETTINGS_TYPE_HISTOGRAM_INVALID;
  }
  NOTREACHED();
  return CONTENT_SETTINGS_TYPE_HISTOGRAM_INVALID;
}

ContentSettingPatternSource::ContentSettingPatternSource(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSetting setting,
    const std::string& source,
    bool incognito)
    : primary_pattern(primary_pattern),
      secondary_pattern(secondary_pattern),
      setting(setting),
      source(source),
      incognito(incognito) {}

ContentSettingPatternSource::ContentSettingPatternSource()
    : setting(CONTENT_SETTING_DEFAULT), incognito(false) {
}

RendererContentSettingRules::RendererContentSettingRules() {}

RendererContentSettingRules::~RendererContentSettingRules() {}
