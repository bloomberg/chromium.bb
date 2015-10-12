// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_INFO_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_INFO_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings.h"

namespace content_settings {

class WebsiteSettingsInfo;

class ContentSettingsInfo {
 public:
  // This object does not take ownership of |website_settings_info|.
  ContentSettingsInfo(const WebsiteSettingsInfo* website_settings_info,
                      const std::vector<std::string>& whitelisted_schemes,
                      const std::set<ContentSetting>& valid_settings);
  ~ContentSettingsInfo();

  const WebsiteSettingsInfo* website_settings_info() const {
    return website_settings_info_;
  }
  const std::vector<std::string>& whitelisted_schemes() const {
    return whitelisted_schemes_;
  }

  bool IsSettingValid(ContentSetting setting) const;

 private:
  const WebsiteSettingsInfo* website_settings_info_;
  const std::vector<std::string> whitelisted_schemes_;
  const std::set<ContentSetting> valid_settings_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsInfo);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_INFO_H_
