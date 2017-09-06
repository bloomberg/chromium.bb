// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/counters/site_settings_counter.h"

#include <set>
#include "build/build_config.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

#if !defined(OS_ANDROID)
#include "content/public/browser/host_zoom_map.h"
#endif

namespace browsing_data {

SiteSettingsCounter::SiteSettingsCounter(HostContentSettingsMap* map,
                                         content::HostZoomMap* zoom_map)
    : map_(map), zoom_map_(zoom_map) {
  DCHECK(map_);
#if !defined(OS_ANDROID)
  DCHECK(zoom_map_);
#else
  DCHECK(!zoom_map_);
#endif
}

SiteSettingsCounter::~SiteSettingsCounter() {}

void SiteSettingsCounter::OnInitialized() {}

const char* SiteSettingsCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteSiteSettings;
}

void SiteSettingsCounter::Count() {
  std::set<std::string> hosts;
  int empty_host_pattern = 0;
  base::Time period_start = GetPeriodStart();
  auto* registry = content_settings::ContentSettingsRegistry::GetInstance();
  for (const content_settings::ContentSettingsInfo* info : *registry) {
    ContentSettingsType type = info->website_settings_info()->type();
    ContentSettingsForOneType content_settings_list;
    map_->GetSettingsForOneType(type, content_settings::ResourceIdentifier(),
                                &content_settings_list);
    for (const auto& content_setting : content_settings_list) {
      // TODO(crbug.com/762560): Check the conceptual SettingSource instead of
      // ContentSettingPatternSource.source
      if (content_setting.source == "preference" ||
          content_setting.source == "notification_android") {
        base::Time last_modified = map_->GetSettingLastModifiedDate(
            content_setting.primary_pattern, content_setting.secondary_pattern,
            type);
        if (last_modified >= period_start) {
          if (content_setting.primary_pattern.GetHost().empty())
            empty_host_pattern++;
          else
            hosts.insert(content_setting.primary_pattern.GetHost());
        }
      }
    }
  }

#if !defined(OS_ANDROID)
  for (const auto& zoom_level : zoom_map_->GetAllZoomLevels()) {
    // zoom_level with non-empty scheme are only used for some internal
    // features and not stored in preferences. They are not counted.
    if (zoom_level.last_modified >= period_start && zoom_level.scheme.empty()) {
      hosts.insert(zoom_level.host);
    }
  }
#endif

  ReportResult(hosts.size() + empty_host_pattern);
}

}  // namespace browsing_data
