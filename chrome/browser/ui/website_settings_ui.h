// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_UI_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "ui/gfx/native_widget_types.h"

class GURL;
class Profile;
class TabContentsWrapper;

namespace content {
struct SSLStatus;
}

class CookieInfoList;
class PermissionInfoList;


class WebsiteSettingsUIDelegate {
 public:
  virtual ~WebsiteSettingsUIDelegate() {}

  // This method is called when the WebsiteSettingsUI is closed.
  virtual void OnUIClosing() = 0;

  // This method is called when ever a permission setting is changed.
  virtual void OnSitePermissionChanged(ContentSettingsType type,
                                       ContentSetting value) = 0;
};

// The UI for website settings displays information and controlls for site
// specific data (local stored objects like cookies), site specific permissions
// (location, popup, plugin, ... permissions) and site specific information
// (identity, connection status, ...). |WebsiteSettingsUI| specifies the
// platform independent interface of the UI.
class WebsiteSettingsUI {
 public:
  struct CookieInfo {
    std::string text;
    int allowed;
    int blocked;
  };

  struct PermissionInfo {
    ContentSettingsType type;
    ContentSetting setting;
  };

  virtual ~WebsiteSettingsUI() {}

  virtual void SetDelegate(WebsiteSettingsUIDelegate* delegate) = 0;

  // Sets site information.
  virtual void SetSiteInfo(const std::string site_info) = 0;

  // Sets cookie information.
  virtual void SetCookieInfo(const CookieInfoList& cookie_info_list) = 0;

  // Sets permision information.
  virtual void SetPermissionInfo(
      const PermissionInfoList& permission_info_list) = 0;
};

class CookieInfoList : public std::vector<WebsiteSettingsUI::CookieInfo> {
};

class PermissionInfoList
    : public std::vector<WebsiteSettingsUI::PermissionInfo> {
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_UI_H_
