// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_WEBSITE_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_WEBSITE_SETTINGS_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/local_shared_objects_container.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace options {

class WebsiteSettingsHandler : public OptionsPageUIHandler {
 public:
  WebsiteSettingsHandler();
  virtual ~WebsiteSettingsHandler();

  typedef std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>
      LocalStorageList;

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Update the page with all origins for a given content setting.
  // |args| is the string name of the content setting.
  void HandleUpdateOrigins(const base::ListValue* args);

  // Update the page with all origins given a filter string.
  // |args| is the filter string.
  void HandleUpdateSearchResults(const base::ListValue* args);

  // Update the page with all origins that are using local storage.
  void HandleUpdateLocalStorage(const base::ListValue* args);

  // Callback method to be invoked when fetching the data is complete.
  void OnLocalStorageFetched(const LocalStorageList& storage);

  // Get all origins with Content Settings for the last given content setting,
  // filter them by |last_filter_|, and update the page.
  void UpdateOrigins();

  // Get all origins with local storage usage, filter them by |last_filter_|,
  // and update the page.
  void UpdateLocalStorage();

  std::string last_setting_;
  std::string last_filter_;
  scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_;
  LocalStorageList local_storage_list_;

  base::WeakPtrFactory<WebsiteSettingsHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_WEBSITE_SETTINGS_HANDLER_H_
