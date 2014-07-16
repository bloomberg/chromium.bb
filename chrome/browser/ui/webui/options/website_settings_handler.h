// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_WEBSITE_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_WEBSITE_SETTINGS_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace options {

class WebsiteSettingsHandler : public OptionsPageUIHandler {
 public:
  WebsiteSettingsHandler();
  virtual ~WebsiteSettingsHandler();

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

  // Get all origins with Content Settings for the last given content setting,
  // filter them by |filter|, and update the page.
  void UpdateOrigins(const std::string& filter);

  ContentSettingsType last_setting_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_WEBSITE_SETTINGS_HANDLER_H_
