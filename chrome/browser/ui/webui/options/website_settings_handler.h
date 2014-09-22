// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_WEBSITE_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_WEBSITE_SETTINGS_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/local_shared_objects_container.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/power/origin_power_map.h"

namespace options {

class WebsiteSettingsHandler : public content_settings::Observer,
                               public OptionsPageUIHandler {
 public:
  WebsiteSettingsHandler();
  virtual ~WebsiteSettingsHandler();

  typedef std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>
      LocalStorageList;

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // content_settings::Observer implementation.
  virtual void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      std::string resource_identifier) OVERRIDE;
  virtual void OnContentSettingUsed(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type) OVERRIDE;

 private:
  // Update the page with all origins for a given content setting.
  // |args| is the string name of the content setting.
  void HandleUpdateOrigins(const base::ListValue* args);

  // Update the page with all origins given a filter string.
  // |args| is the filter string.
  void HandleUpdateSearchResults(const base::ListValue* args);

  // Update the single site edit view with the permission values for a given
  // url, if the url is valid.
  // |args| is the URL.
  void HandleGetOriginInfo(const base::ListValue* args);

  // Sets the content setting permissions for a given setting type for the last
  // used origin.
  // |args| is the name of the setting and the new value.
  void HandleSetOriginPermission(const base::ListValue* args);

  // Update the page with all origins that are using local storage.
  void HandleUpdateLocalStorage(const base::ListValue* args);

  // Show the single site edit view if the given URL is valid.
  // |args| is the URL.
  void HandleMaybeShowEditPage(const base::ListValue* args);

  // Get all origins that have used power, filter them by |last_filter_|, and
  // update the page.
  void HandleUpdateBatteryUsage(const base::ListValue* args);

  // Deletes the local storage and repopulates the page.
  void HandleDeleteLocalStorage(const base::ListValue* args);

  // Populates the default setting drop down on the single site edit page.
  void HandleUpdateDefaultSetting(const base::ListValue* args);

  // Sets the default setting for the last used content setting to |args|.
  void HandleSetDefaultSetting(const base::ListValue* args);

  // Sets if a certain content setting enabled to |args|.
  void HandleSetGlobalToggle(const base::ListValue* args);

  // Closes all tabs and app windows which have the same origin as the selected
  // page.
  void HandleStopOrigin(const base::ListValue* args);

  // Callback method to be invoked when fetching the data is complete.
  void OnLocalStorageFetched(const LocalStorageList& storage);

  // Get all origins with Content Settings for the last given content setting,
  // filter them by |last_filter_|, and update the page.
  void UpdateOrigins();

  // Get all origins with local storage usage, filter them by |last_filter_|,
  // and update the page.
  void UpdateLocalStorage();

  // Get all origins with power consumption, filter them by |last_filter_|,
  // and update the page.
  void UpdateBatteryUsage();

  // Kill all tabs and app windows which have the same origin as |site_url|.
  void StopOrigin(const GURL& site_url);

  // Delete all of the local storage for the |site_url|.
  void DeleteLocalStorage(const GURL& site_url);

  // Populates the single site edit view with the permissions and local storage
  // usage for a given |site_url|. If |show_page| is true, it raises a new
  // single site edit view.
  void GetInfoForOrigin(const GURL& site_url, bool show_page);

  // Updates the page with the last settings used.
  void Update();

  // Gets the default setting in string form. If |provider_id| is not NULL, the
  // id of the provider which provided the default setting is assigned to it.
  std::string GetSettingDefaultFromModel(ContentSettingsType type,
                                         std::string* provider_id);

  // Returns the base URL for websites, or the app name for Chrome App URLs.
  const std::string& GetReadableName(const GURL& site_url);

  Profile* GetProfile();

  std::string last_setting_;
  std::string last_filter_;
  GURL last_site_;
  scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_;
  LocalStorageList local_storage_list_;

  // Observer to watch for content settings changes.
  ScopedObserver<HostContentSettingsMap, content_settings::Observer> observer_;

  // Subscription to watch for power consumption updates.
  scoped_ptr<power::OriginPowerMap::Subscription> subscription_;

  base::WeakPtrFactory<WebsiteSettingsHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_WEBSITE_SETTINGS_HANDLER_H_
