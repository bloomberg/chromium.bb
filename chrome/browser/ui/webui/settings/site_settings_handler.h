// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_HANDLER_H_

#include <vector>

#include "base/scoped_observer.h"
#include "chrome/browser/storage/storage_info_fetcher.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/content_settings/core/browser/content_settings_observer.h"

class HostContentSettingsMap;
class Profile;

namespace base {
class ListValue;
}

namespace settings {

// Chrome "ContentSettings" settings page UI handler.
class SiteSettingsHandler : public SettingsPageUIHandler,
                            public content_settings::Observer {
 public:
  explicit SiteSettingsHandler(Profile* profile);
  ~SiteSettingsHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;

  void OnGetUsageInfo(const storage::UsageInfoEntries& entries);
  void OnUsageInfoCleared(storage::QuotaStatusCode code);

  // content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               std::string resource_identifier) override;
 private:
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, GetAndSetDefault);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, Origins);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, Patterns);

  // Asynchronously fetches the usage for a given origin. Replies back with
  // OnGetUsageInfo above.
  void HandleFetchUsageTotal(const base::ListValue* args);

  // Deletes the storage being used for a given host.
  void HandleClearUsage(const base::ListValue* args);

  // Gets and sets the default value for a particular content settings type.
  void HandleSetDefaultValueForContentType(const base::ListValue* args);
  void HandleGetDefaultValueForContentType(const base::ListValue* args);

  // Returns the list of site exceptions for a given content settings type.
  void HandleGetExceptionList(const base::ListValue* args);

  // Handles setting and resetting of an origin permission.
  void HandleResetCategoryPermissionForOrigin(const base::ListValue* args);
  void HandleSetCategoryPermissionForOrigin(const base::ListValue* args);

  // Returns whether a given pattern is valid.
  void HandleIsPatternValid(const base::ListValue* args);

  Profile* profile_;

  // The host for which to fetch usage.
  std::string usage_host_;

  // The origin for which to clear usage.
  std::string clearing_origin_;

  // Change observer for content settings.
  ScopedObserver<HostContentSettingsMap, content_settings::Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(SiteSettingsHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_HANDLER_H_
