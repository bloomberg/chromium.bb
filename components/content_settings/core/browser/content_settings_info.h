// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_INFO_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_INFO_H_

#include <string>
#include <vector>

#include "base/macros.h"

namespace content_settings {

class WebsiteSettingsInfo;

class ContentSettingsInfo {
 public:
  // This object does not take ownership of |website_settings_info|.
  ContentSettingsInfo(const WebsiteSettingsInfo* website_settings_info,
                      const std::vector<std::string>& whitelisted_schemes);
  ~ContentSettingsInfo();

  const WebsiteSettingsInfo* website_settings_info() const {
    return website_settings_info_;
  }
  const std::vector<std::string>& whitelisted_schemes() const {
    return whitelisted_schemes_;
  }

 private:
  const WebsiteSettingsInfo* website_settings_info_;
  std::vector<std::string> whitelisted_schemes_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsInfo);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_INFO_H_
