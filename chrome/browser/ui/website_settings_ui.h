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

class CookieInfoList;
class GURL;
class PermissionInfoList;
class Profile;
class TabContentsWrapper;
class WebsiteSettings;

namespace content {
struct SSLStatus;
}

// The class |WebsiteSettingsUI| specifies the platform independent
// interface of the website settings UI. The website settings UI displays
// information and controls for site specific data (local stored objects like
// cookies), site specific permissions (location, popup, plugin, etc.
// permissions) and site specific information (identity, connection status,
// etc.).
class WebsiteSettingsUI {
 public:
  // |CookieInfo| contains information about the cookies from a specific source.
  // A source can for example be a specific origin or an entire domain.
  struct CookieInfo {
    CookieInfo();

    // String describing the cookie source.
    std::string cookie_source;
    // The number of allowed cookies.
    int allowed;
    // The number of blocked cookies.
    int blocked;
  };

  // |PermissionInfo| contains information about a single permission |type| for
  // the current website.
  struct PermissionInfo {
    PermissionInfo();

    // Site permission |type|.
    ContentSettingsType type;
    // The current value for the permission |type| (e.g. ALLOW or BLOCK).
    ContentSetting setting;
    // The global default settings for this permission |type|.
    ContentSetting default_setting;
  };

  virtual ~WebsiteSettingsUI();

  // Sets the |presenter| of the WebsiteSettingsUI that is responsible for
  // setting the data to display in the UI.
  virtual void SetPresenter(WebsiteSettings* presenter) = 0;

  // Sets site information.
  virtual void SetSiteInfo(const std::string& site_info) = 0;

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
