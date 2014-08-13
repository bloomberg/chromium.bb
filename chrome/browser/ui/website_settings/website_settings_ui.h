// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_WEBSITE_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_WEBSITE_SETTINGS_UI_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/ui/website_settings/website_settings.h"
#include "chrome/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/common/signed_certificate_timestamp_id_and_status.h"
#include "ui/gfx/native_widget_types.h"


class GURL;
class Profile;
class WebsiteSettings;
namespace content {
struct SSLStatus;
}

namespace gfx {
class Image;
}

// The class |WebsiteSettingsUI| specifies the platform independent
// interface of the website settings UI. The website settings UI displays
// information and controls for site specific data (local stored objects like
// cookies), site specific permissions (location, popup, plugin, etc.
// permissions) and site specific information (identity, connection status,
// etc.).
class WebsiteSettingsUI {
 public:
  // The Website Settings UI contains several tabs. Each tab is assiciated with
  // a unique tab id. The enum |TabId| contains all the ids for the tabs.
  enum TabId {
    TAB_ID_PERMISSIONS = 0,
    TAB_ID_CONNECTION,
    NUM_TAB_IDS,
  };

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
    // The settings source e.g. user, extensions, policy, ... .
    content_settings::SettingSource source;
  };

  // |IdentityInfo| contains information about the site's identity and
  // connection.
  struct IdentityInfo {
    IdentityInfo();
    ~IdentityInfo();

    // The site's identity.
    std::string site_identity;
    // Status of the site's identity.
    WebsiteSettings::SiteIdentityStatus identity_status;
    // Helper to get the status text to display to the user.
    base::string16 GetIdentityStatusText() const;
    // Textual description of the site's identity status that is displayed to
    // the user.
    std::string identity_status_description;
    // The ID is the server certificate of a secure connection or 0.
    int cert_id;
    // Signed Certificate Timestamp ids and status
    content::SignedCertificateTimestampIDStatusList
        signed_certificate_timestamp_ids;
    // Status of the site's connection.
    WebsiteSettings::SiteConnectionStatus connection_status;
    // Textual description of the site's connection status that is displayed to
    // the user.
    std::string connection_status_description;
  };

  typedef std::vector<CookieInfo> CookieInfoList;
  typedef std::vector<PermissionInfo> PermissionInfoList;

  virtual ~WebsiteSettingsUI();

  // Returns the UI string for the given permission |type|.
  static base::string16 PermissionTypeToUIString(ContentSettingsType type);

  // Returns the UI string for the given permission |value|, used in the
  // permission-changing menu. Generally this will be a verb in the imperative
  // form, e.g. "ask", "allow", "block".
  static base::string16 PermissionValueToUIString(ContentSetting value);

  // Returns the UI string describing the action taken for a permission,
  // including why that action was taken. E.g. "Allowed by you",
  // "Blocked by default".
  static base::string16 PermissionActionToUIString(
      ContentSetting setting,
      ContentSetting default_setting,
      content_settings::SettingSource source);

  // Returns the icon resource ID for the given permission |type| and |setting|.
  static int GetPermissionIconID(ContentSettingsType type,
                                 ContentSetting setting);

  // Returns the icon for the given permissionInfo |info|.  If |info|'s current
  // setting is CONTENT_SETTING_DEFAULT, it will return the icon for |info|'s
  // default setting.
  static const gfx::Image& GetPermissionIcon(const PermissionInfo& info);

  // Returns the identity icon ID for the given identity |status|.
  static int GetIdentityIconID(WebsiteSettings::SiteIdentityStatus status);

  // Returns the identity icon for the given identity |status|.
  static const gfx::Image& GetIdentityIcon(
      WebsiteSettings::SiteIdentityStatus status);

  // Returns the connection icon ID for the given connection |status|.
  static int GetConnectionIconID(
      WebsiteSettings::SiteConnectionStatus status);

  // Returns the connection icon for the given connection |status|.
  static const gfx::Image& GetConnectionIcon(
      WebsiteSettings::SiteConnectionStatus status);

  // Returns the icon ID to show along with the first visit information.
  static int GetFirstVisitIconID(const base::string16& first_visit);

  // Returns the icon to show along with the first visit information.
  static const gfx::Image& GetFirstVisitIcon(const base::string16& first_visit);

  // Sets cookie information.
  virtual void SetCookieInfo(const CookieInfoList& cookie_info_list) = 0;

  // Sets permision information.
  virtual void SetPermissionInfo(
      const PermissionInfoList& permission_info_list) = 0;

  // Sets site identity information.
  virtual void SetIdentityInfo(const IdentityInfo& identity_info) = 0;

  // Sets the first visited data. |first_visit| can be an empty string.
  virtual void SetFirstVisit(const base::string16& first_visit) = 0;

  // Selects the tab with the given |tab_id|.
  virtual void SetSelectedTab(TabId tab_id) = 0;
};

typedef WebsiteSettingsUI::CookieInfoList CookieInfoList;
typedef WebsiteSettingsUI::PermissionInfoList PermissionInfoList;

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_WEBSITE_SETTINGS_UI_H_
