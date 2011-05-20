// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_provider.h"

namespace content_settings {

ProviderInterface::Rule::Rule() {
}

ProviderInterface::Rule::Rule(const ContentSettingsPattern& requesting_pattern,
                              const ContentSettingsPattern& embedding_pattern,
                              ContentSetting setting)
    : requesting_url_pattern(requesting_pattern),
      embedding_url_pattern(embedding_pattern),
      content_setting(setting) {
}

}  // namespace content_settings
