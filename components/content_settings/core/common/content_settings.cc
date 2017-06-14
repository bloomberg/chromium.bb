// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings.h"

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "build/build_config.h"

ContentSetting IntToContentSetting(int content_setting) {
  return ((content_setting < 0) ||
          (content_setting >= CONTENT_SETTING_NUM_SETTINGS)) ?
      CONTENT_SETTING_DEFAULT : static_cast<ContentSetting>(content_setting);
}

struct HistogramValue {
  ContentSettingsType type;
  int value;
};

// WARNING: The value specified here for a type should match exactly the value
// specified in the ContentType enum in histograms.xml. Since these values are
// used for histograms, please do not reuse the same value for a different
// content setting. Always append to the end and increment.
// TODO(raymes): We should use a sparse histogram here on the hash of the
// content settings type name instead.
HistogramValue kHistogramValue[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, 0},
    {CONTENT_SETTINGS_TYPE_IMAGES, 1},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, 2},
    {CONTENT_SETTINGS_TYPE_PLUGINS, 3},
    {CONTENT_SETTINGS_TYPE_POPUPS, 4},
    {CONTENT_SETTINGS_TYPE_GEOLOCATION, 5},
    {CONTENT_SETTINGS_TYPE_NOTIFICATIONS, 6},
    {CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE, 7},
    {CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, 10},
    {CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, 12},
    {CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, 13},
    {CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS, 14},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, 15},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, 16},
    {CONTENT_SETTINGS_TYPE_MIDI_SYSEX, 17},
    {CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, 19},
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    {CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER, 21},
#endif
    {CONTENT_SETTINGS_TYPE_APP_BANNER, 22},
    {CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, 23},
    {CONTENT_SETTINGS_TYPE_DURABLE_STORAGE, 24},
    {CONTENT_SETTINGS_TYPE_BLUETOOTH_GUARD, 26},
    {CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC, 27},
    {CONTENT_SETTINGS_TYPE_AUTOPLAY, 28},
    {CONTENT_SETTINGS_TYPE_IMPORTANT_SITE_INFO, 30},
    {CONTENT_SETTINGS_TYPE_PERMISSION_AUTOBLOCKER_DATA, 31},
    {CONTENT_SETTINGS_TYPE_ADS, 32},
    {CONTENT_SETTINGS_TYPE_ADS_DATA, 33},
    {CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, 34},
};

int ContentSettingTypeToHistogramValue(ContentSettingsType content_setting,
                                       size_t* num_values) {
  // Translate the list above into a map for fast lookup.
  typedef base::hash_map<int, int> Map;
  CR_DEFINE_STATIC_LOCAL(Map, kMap, ());
  if (kMap.empty()) {
    for (const HistogramValue& histogram_value : kHistogramValue)
      kMap[histogram_value.type] = histogram_value.value;
  }

  DCHECK(base::ContainsKey(kMap, content_setting));
  *num_values = arraysize(kHistogramValue);
  return kMap[content_setting];
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

ContentSettingPatternSource::ContentSettingPatternSource(
    const ContentSettingPatternSource& other) = default;

RendererContentSettingRules::RendererContentSettingRules() {}

RendererContentSettingRules::~RendererContentSettingRules() {}
