// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_INFO_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_INFO_H_

#include <string>

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content_settings {

// This class stores the properties related to a website setting.
// TODO(raymes): Move more properties into this class.
class WebsiteSettingsInfo {
 public:
  WebsiteSettingsInfo(ContentSettingsType type, const std::string& name);
  ~WebsiteSettingsInfo();

  ContentSettingsType type() const { return type_; }
  const std::string& name() const { return name_; }

 private:
  const ContentSettingsType type_;
  const std::string name_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsInfo);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_INFO_H_
