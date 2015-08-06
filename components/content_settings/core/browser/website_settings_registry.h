// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_REGISTRY_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_REGISTRY_H_

#include <string>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content_settings {

// This class stores WebsiteSettingsInfo objects for each website setting in the
// system and provides access to them. Global instances can be fetched and
// methods called from from any thread because all of its public methods are
// const.
class WebsiteSettingsRegistry {
 public:
  static WebsiteSettingsRegistry* GetInstance();

  const WebsiteSettingsInfo* Get(ContentSettingsType type) const;
  const WebsiteSettingsInfo* GetByName(const std::string& name) const;

 private:
  friend class WebsiteSettingsRegistryTest;
  friend struct base::DefaultLazyInstanceTraits<WebsiteSettingsRegistry>;

  WebsiteSettingsRegistry();
  ~WebsiteSettingsRegistry();

  // Register a new website setting. This maps an arbitrary base::Value to an
  // origin.
  void RegisterWebsiteSetting(ContentSettingsType type,
                              const std::string& name);
  // Register a new content setting. This maps an ALLOW/ASK/BLOCK value (see the
  // ContentSetting enum) to an origin.
  void RegisterContentSetting(ContentSettingsType type,
                              const std::string& name,
                              ContentSetting initial_default_value);

  // Helper used by Register/RegisterPermission.
  void StoreWebsiteSettingsInfo(WebsiteSettingsInfo* info);

  ScopedVector<WebsiteSettingsInfo> website_settings_info_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsRegistry);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_REGISTRY_H_
