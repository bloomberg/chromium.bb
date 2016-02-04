// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_REGISTRY_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_REGISTRY_H_

#include <map>
#include <string>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
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
  using Map = std::map<ContentSettingsType, scoped_ptr<WebsiteSettingsInfo>>;
  using const_iterator = MapValueIterator<typename Map::const_iterator,
                                          const WebsiteSettingsInfo*>;

  static WebsiteSettingsRegistry* GetInstance();

  // Reset the instance for use inside tests.
  void ResetForTest();

  const WebsiteSettingsInfo* Get(ContentSettingsType type) const;
  const WebsiteSettingsInfo* GetByName(const std::string& name) const;

  // Register a new website setting. This maps an origin to an arbitrary
  // base::Value. Returns a pointer to the registered WebsiteSettingsInfo which
  // is owned by the registry.
  const WebsiteSettingsInfo* Register(
      ContentSettingsType type,
      const std::string& name,
      scoped_ptr<base::Value> initial_default_value,
      WebsiteSettingsInfo::SyncStatus sync_status,
      WebsiteSettingsInfo::LossyStatus lossy_status,
      WebsiteSettingsInfo::ScopingType scoping_type,
      WebsiteSettingsInfo::IncognitoBehavior incognito_behavior);

  const_iterator begin() const;
  const_iterator end() const;

 private:
  friend class ContentSettingsRegistryTest;
  friend class WebsiteSettingsRegistryTest;
  friend struct base::DefaultLazyInstanceTraits<WebsiteSettingsRegistry>;

  WebsiteSettingsRegistry();
  ~WebsiteSettingsRegistry();

  void Init();

  Map website_settings_info_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsRegistry);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_REGISTRY_H_
