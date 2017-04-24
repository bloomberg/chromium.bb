// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_CONTENT_SETTINGS_MANAGER_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_CONTENT_SETTINGS_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings_types.h"

class ContentSettingsPattern;
class HostContentSettingsMap;
class Profile;

// This class observes subresource filter content settings changes for metrics
// collection.
class SubresourceFilterContentSettingsManager
    : public content_settings::Observer {
 public:
  explicit SubresourceFilterContentSettingsManager(Profile* profile);
  ~SubresourceFilterContentSettingsManager() override;

 private:
  // content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               std::string resource_identifier) override;

  HostContentSettingsMap* settings_map_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterContentSettingsManager);
};

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_CONTENT_SETTINGS_MANAGER_H_
