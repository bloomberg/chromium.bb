// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/site_settings_counter.h"

#include <set>
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

namespace browsing_data {

SiteSettingsCounter::SiteSettingsCounter(HostContentSettingsMap* map)
    : map_(map) {
  DCHECK(map_);
}

SiteSettingsCounter::~SiteSettingsCounter() {}

void SiteSettingsCounter::OnInitialized() {}

const char* SiteSettingsCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteSiteSettings;
}

void SiteSettingsCounter::Count() {
  std::set<ContentSettingsPattern> patterns;
  base::Time period_start = GetPeriodStart();
  auto* registry = content_settings::ContentSettingsRegistry::GetInstance();
  for (const content_settings::ContentSettingsInfo* info : *registry) {
    ContentSettingsType type = info->website_settings_info()->type();
    ContentSettingsForOneType content_settings_list;
    map_->GetSettingsForOneType(type, content_settings::ResourceIdentifier(),
                                &content_settings_list);
    for (const auto& content_setting : content_settings_list) {
      if (content_setting.source == "preference") {
        base::Time last_modified = map_->GetSettingLastModifiedDate(
            content_setting.primary_pattern, content_setting.secondary_pattern,
            type);
        if (last_modified >= period_start) {
          patterns.insert(content_setting.primary_pattern);
        }
      }
    }
  }
  ReportResult(patterns.size());
}

}  // namespace browsing_data
