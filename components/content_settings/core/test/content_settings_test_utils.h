// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_TEST_CONTENT_SETTINGS_TEST_UTILS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_TEST_CONTENT_SETTINGS_TEST_UTILS_H_

#include "components/content_settings/core/browser/content_settings_utils.h"

namespace content_settings {

// The following two functions return the content setting (represented as
// Value or directly the ContentSetting enum) from |provider| for the
// given |content_type| and |resource_identifier|. The returned content setting
// applies to the primary and secondary URL, and to the normal or incognito
// mode, depending on |include_incognito|.
base::Value* GetContentSettingValue(const ProviderInterface* provider,
                                    const GURL& primary_url,
                                    const GURL& secondary_url,
                                    ContentSettingsType content_type,
                                    const std::string& resource_identifier,
                                    bool include_incognito);

ContentSetting GetContentSetting(const ProviderInterface* provider,
                                 const GURL& primary_url,
                                 const GURL& secondary_url,
                                 ContentSettingsType content_type,
                                 const std::string& resource_identifier,
                                 bool include_incognito);

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_TEST_CONTENT_SETTINGS_TEST_UTILS_H_
