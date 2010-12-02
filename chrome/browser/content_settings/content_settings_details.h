// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The details send with notifications about content setting changes.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_DETAILS_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_DETAILS_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/content_settings/content_settings_pattern.h"
#include "chrome/common/content_settings.h"

// Details for the CONTENT_SETTINGS_CHANGED notification. This is sent when
// content settings change for at least one host. If settings change for more
// than one pattern in one user interaction, this will usually send a single
// notification with update_all() returning true instead of one notification
// for each pattern.
class ContentSettingsDetails {
 public:
  // Update the setting that matches this pattern/content type/resource.
  ContentSettingsDetails(const ContentSettingsPattern& pattern,
                         ContentSettingsType type,
                         const std::string& resource_identifier)
      : pattern_(pattern),
        type_(type),
        resource_identifier_(resource_identifier) {}

  // The pattern whose settings have changed.
  const ContentSettingsPattern& pattern() const { return pattern_; }

  // True if all settings should be updated for the given type.
  bool update_all() const { return pattern_.AsString().empty(); }

  // The type of the pattern whose settings have changed.
  ContentSettingsType type() const { return type_; }

  // The resource identifier for the settings type that has changed.
  const std::string& resource_identifier() const {
    return resource_identifier_;
  }

  // True if all types should be updated. If update_all() is false, this will
  // be false as well (although the reverse does not hold true).
  bool update_all_types() const {
    return CONTENT_SETTINGS_TYPE_DEFAULT == type_;
  }

 private:
  ContentSettingsPattern pattern_;
  ContentSettingsType type_;
  std::string resource_identifier_;
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_DETAILS_H_
