// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/test/content_settings_test_utils.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content_settings {

base::Value* GetContentSettingValue(const ProviderInterface* provider,
                                    const GURL& primary_url,
                                    const GURL& secondary_url,
                                    ContentSettingsType content_type,
                                    const std::string& resource_identifier,
                                    bool include_incognito) {
  return GetContentSettingValueAndPatterns(provider, primary_url, secondary_url,
                                           content_type, resource_identifier,
                                           include_incognito, NULL, NULL);
}

ContentSetting GetContentSetting(const ProviderInterface* provider,
                                 const GURL& primary_url,
                                 const GURL& secondary_url,
                                 ContentSettingsType content_type,
                                 const std::string& resource_identifier,
                                 bool include_incognito) {
  scoped_ptr<base::Value> value(
      GetContentSettingValue(provider, primary_url, secondary_url, content_type,
                             resource_identifier, include_incognito));
  return ValueToContentSetting(value.get());
}

}  // namespace content_settings
