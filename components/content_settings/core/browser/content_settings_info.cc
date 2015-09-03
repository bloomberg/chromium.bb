// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_info.h"

namespace content_settings {

ContentSettingsInfo::ContentSettingsInfo(
    const WebsiteSettingsInfo* website_settings_info,
    const std::vector<std::string>& whitelisted_schemes)
    : website_settings_info_(website_settings_info),
      whitelisted_schemes_(whitelisted_schemes) {}

ContentSettingsInfo::~ContentSettingsInfo() {}

}  // namespace content_settings
