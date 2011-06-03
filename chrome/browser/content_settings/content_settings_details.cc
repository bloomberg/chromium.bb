// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_details.h"

ContentSettingsDetails::ContentSettingsDetails(
    const ContentSettingsPattern& pattern,
    ContentSettingsType type,
    const std::string& resource_identifier)
    : pattern_(pattern),
      type_(type),
      resource_identifier_(resource_identifier) {
}

