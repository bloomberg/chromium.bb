// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources_util.h"

#include <stddef.h>

#include <utility>

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "build/build_config.h"
#include "chrome/grit/theme_resources_map.h"
#include "components/grit/components_scaled_resources_map.h"
#include "ui/resources/grit/ui_resources_map.h"

#if defined(OS_CHROMEOS)
#include "ui/chromeos/resources/grit/ui_chromeos_resources_map.h"
#endif

namespace {

// A wrapper class that holds a hash_map between resource strings and resource
// ids.  This is done so we can use base::LazyInstance which takes care of
// thread safety in initializing the hash_map for us.
class ThemeMap {
 public:
  typedef base::hash_map<std::string, int> StringIntMap;

  ThemeMap() {
    for (size_t i = 0; i < kComponentsScaledResourcesSize; ++i) {
      id_map_[kComponentsScaledResources[i].name] =
          kComponentsScaledResources[i].value;
    }
    for (size_t i = 0; i < kThemeResourcesSize; ++i)
      id_map_[kThemeResources[i].name] = kThemeResources[i].value;
    for (size_t i = 0; i < kUiResourcesSize; ++i)
      id_map_[kUiResources[i].name] = kUiResources[i].value;
#if defined(OS_CHROMEOS)
    for (size_t i = 0; i < kUiChromeosResourcesSize; ++i)
      id_map_[kUiChromeosResources[i].name] = kUiChromeosResources[i].value;
#endif
  }

  int GetId(const std::string& resource_name) {
    StringIntMap::const_iterator it = id_map_.find(resource_name);
    if (it == id_map_.end())
      return -1;
    return it->second;
  }

 private:
  StringIntMap id_map_;
};

static base::LazyInstance<ThemeMap> g_theme_ids = LAZY_INSTANCE_INITIALIZER;

}  // namespace

int ResourcesUtil::GetThemeResourceId(const std::string& resource_name) {
  return g_theme_ids.Get().GetId(resource_name);
}
