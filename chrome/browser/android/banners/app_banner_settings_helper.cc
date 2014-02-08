// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_settings_helper.h"

#include <string>

#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/content_settings_pattern.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {
// Max number of apps that a particular site may show a banner for.
const size_t kMaxAppsPerSite = 3;
}

bool AppBannerSettingsHelper::IsAllowed(content::WebContents* web_contents,
                                        const GURL& origin_url,
                                        const std::string& package_name) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsOffTheRecord() || web_contents->GetURL() != origin_url ||
      package_name.empty()) {
    return false;
  }

  // Check if this combination has been previously disabled.
  HostContentSettingsMap* settings = profile->GetHostContentSettingsMap();
  if (!settings)
    return false;
  scoped_ptr<base::Value> value(
      settings->GetWebsiteSetting(origin_url,
                                  origin_url,
                                  CONTENT_SETTINGS_TYPE_APP_BANNER,
                                  std::string(),
                                  NULL));
  if (!value.get()) {
    // We've never blocked a banner on this site.
    return true;
  } else if (value->IsType(base::Value::TYPE_DICTIONARY)) {
    // We expect to get dictionary back, where the keys are the package names.
    base::DictionaryValue* banner_dict =
        static_cast<base::DictionaryValue*>(value.get());
    bool is_allowed = false;
    if (banner_dict->GetBoolean(package_name, &is_allowed)) {
      return is_allowed;
    } else {
      return banner_dict->size() < ::kMaxAppsPerSite;
    }
  } else {
    // Somehow the value we got back is not a dictionary (e.g. the settings were
    // corrupted by malware).  Try to clear it out.
    ContentSettingsPattern pattern(ContentSettingsPattern::FromURL(origin_url));
    if (pattern.IsValid()) {
      settings->SetWebsiteSetting(pattern,
                                  ContentSettingsPattern::Wildcard(),
                                  CONTENT_SETTINGS_TYPE_APP_BANNER,
                                  std::string(),
                                  NULL);
      return true;
    }
  }

  return false;
}

void AppBannerSettingsHelper::Block(content::WebContents* web_contents,
                                    const GURL& origin_url,
                                    const std::string& package_name) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* settings = profile->GetHostContentSettingsMap();
  ContentSettingsPattern pattern(ContentSettingsPattern::FromURL(origin_url));
  if (!settings || !pattern.IsValid())
    return;

  scoped_ptr<base::Value> value(
      settings->GetWebsiteSetting(origin_url,
                                  origin_url,
                                  CONTENT_SETTINGS_TYPE_APP_BANNER,
                                  std::string(),
                                  NULL));
  base::DictionaryValue* banner_dict = NULL;
  if (value.get() && value->IsType(base::Value::TYPE_DICTIONARY)) {
    banner_dict = static_cast<base::DictionaryValue*>(value.release());
  } else {
    banner_dict = new base::DictionaryValue();
  }

  // Update the setting and save it back.
  banner_dict->SetBoolean(package_name, false);
  settings->SetWebsiteSetting(pattern,
                              ContentSettingsPattern::Wildcard(),
                              CONTENT_SETTINGS_TYPE_APP_BANNER,
                              std::string(),
                              banner_dict);
}
