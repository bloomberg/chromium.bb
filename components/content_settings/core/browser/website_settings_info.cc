// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/website_settings_info.h"

namespace content_settings {

WebsiteSettingsInfo::WebsiteSettingsInfo(ContentSettingsType type,
                                         const std::string& name)
    : type_(type), name_(name) {}

WebsiteSettingsInfo::~WebsiteSettingsInfo() {}

}  // namespace content_settings
