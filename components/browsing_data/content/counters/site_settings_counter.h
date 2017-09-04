// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_COUNTERS_SITE_SETTINGS_COUNTER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_COUNTERS_SITE_SETTINGS_COUNTER_H_

#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

namespace content {
class HostZoomMap;
}

namespace browsing_data {

class SiteSettingsCounter : public browsing_data::BrowsingDataCounter {
 public:
  explicit SiteSettingsCounter(HostContentSettingsMap* map,
                               content::HostZoomMap* zoom_map);
  ~SiteSettingsCounter() override;

  const char* GetPrefName() const override;

 private:
  void OnInitialized() override;

  void Count() override;

  scoped_refptr<HostContentSettingsMap> map_;
  content::HostZoomMap* zoom_map_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_COUNTERS_SITE_SETTINGS_COUNTER_H_
