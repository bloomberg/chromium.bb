// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_UI_H_
#define CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_UI_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/ui/page_info/page_info.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "ui/gfx/native_widget_types.h"

#if !defined(OS_ANDROID)
#include "ui/gfx/image/image_skia.h"
#endif

class GURL;
class Profile;
class PageInfo;

namespace gfx {
class Image;
}

namespace net {
class X509Certificate;
}

// The class |PageInfoUI| specifies the platform independent
// interface of the page info UI. The page info UI displays
// information and controls for site specific data (local stored objects like
// cookies), site specific permissions (location, popup, plugin, etc.
// permissions) and site specific information (identity, connection status,
// etc.).
class PageInfoUI {
 public:
  // The Page Info UI contains several tabs. Each tab is associated with
  // a unique tab id. The enum |TabId| contains all the ids for the tabs.
  enum TabId {
    TAB_ID_PERMISSIONS = 0,
    TAB_ID_CONNECTION,
    NUM_TAB_IDS,
  };

  // The security summary is styled depending on the security state. At the
  // moment, the only styling we apply is color, but it could also include e.g.
  // bolding.
  enum SecuritySummaryStyle { STYLE_UNSTYLED = 0, STYLE_COLOR = 1 << 1 };

  struct SecurityDescription {
    // A one-line summary of the security state.
    base::string16 summary;
    // A short paragraph with more details about the state, and how
    // the user should treat it.
    base::string16 details;
  };

  // |CookieInfo| contains information about the cookies from a specific source.
  // A source can for example be a specific origin or an entire wildcard domain.
  struct CookieInfo {
    CookieInfo();

    // The number of allowed cookies.
    int allowed;
    // The number of blocked cookies.
    int blocked;

    // Whether these cookies are from the current top-level origin as seen by
    // the user, or from third-party origins.
    bool is_first_party;
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
    // Whether the profile is off the record.
    bool is_incognito;
  };

  // |ChosenObjectInfo| contains information about a single |object| of a
  // chooser |type| that the current website has been granted access to.
  struct ChosenObjectInfo {
    ChosenObjectInfo(const PageInfo::ChooserUIInfo& ui_info,
                     std::unique_ptr<base::DictionaryValue> object);
    ~ChosenObjectInfo();
    // |ui_info| for this chosen object type.
    const PageInfo::ChooserUIInfo& ui_info;
    // The opaque |object| representing the thing the user selected.
    std::unique_ptr<base::DictionaryValue> object;
  };

  // |IdentityInfo| contains information about the site's identity and
  // connection.
  struct IdentityInfo {
    IdentityInfo();
    ~IdentityInfo();

    // The site's identity: the certificate's Organization Name for sites with
    // Extended Validation certificates, or the URL's hostname for all other
    // sites.
    std::string site_identity;
    // Status of the site's identity.
    PageInfo::SiteIdentityStatus identity_status;
    // Helper to get security description info to display to the user.
    std::unique_ptr<SecurityDescription> GetSecurityDescription() const;
    // Textual description of the site's identity status that is displayed to
    // the user.
    std::string identity_status_description;
    // The server certificate if a secure connection.
    scoped_refptr<net::X509Certificate> certificate;
    // Status of the site's connection.
    PageInfo::SiteConnectionStatus connection_status;
    // Textual description of the site's connection status that is displayed to
    // the user.
    std::string connection_status_description;
    // Set when the user has explicitly bypassed an SSL error for this host and
    // has a flag set to remember ssl decisions (explicit flag or in the
    // experimental group).  When |show_ssl_decision_revoke_button| is true, the
    // connection area of the page info will include an option for the user to
    // revoke their decision to bypass the SSL error for this host.
    bool show_ssl_decision_revoke_button;
    // Set when the user ignored the password reuse modal warning dialog. When
    // |show_change_password_buttons| is true, the page identity area of the
    // page info will include buttons to change corresponding password, and
    // to whitelist current site.
    bool show_change_password_buttons;
  };

  using CookieInfoList = std::vector<CookieInfo>;
  using PermissionInfoList = std::vector<PermissionInfo>;
  using ChosenObjectInfoList = std::vector<std::unique_ptr<ChosenObjectInfo>>;

  virtual ~PageInfoUI();

  // Returns the UI string for the given permission |type|.
  static base::string16 PermissionTypeToUIString(ContentSettingsType type);

  // Returns the UI string describing the action taken for a permission,
  // including why that action was taken. E.g. "Allowed by you",
  // "Blocked by default". If |setting| is default, specify the actual default
  // setting using |default_setting|.
  static base::string16 PermissionActionToUIString(
      Profile* profile,
      ContentSettingsType type,
      ContentSetting setting,
      ContentSetting default_setting,
      content_settings::SettingSource source);

  // Returns a string indicating whether the permission was blocked via an
  // extension, enterprise policy, or embargo.
  static base::string16 PermissionDecisionReasonToUIString(
      Profile* profile,
      const PermissionInfo& permission,
      const GURL& url);

  // Returns the color to use for the permission decision reason strings.
  static SkColor GetPermissionDecisionTextColor();

  // Returns the icon resource ID for the given permission |type| and |setting|.
  static int GetPermissionIconID(ContentSettingsType type,
                                 ContentSetting setting);

  // Returns the icon for the given permissionInfo |info|.  If |info|'s current
  // setting is CONTENT_SETTING_DEFAULT, it will return the icon for |info|'s
  // default setting.
  static const gfx::Image& GetPermissionIcon(const PermissionInfo& info);

  // Returns the UI string describing the given object |info|.
  static base::string16 ChosenObjectToUIString(const ChosenObjectInfo& info);

  // Returns the icon for the given object |info|.
  static const gfx::Image& GetChosenObjectIcon(const ChosenObjectInfo& info,
                                               bool deleted);

#if defined(OS_ANDROID)
  // Returns the identity icon ID for the given identity |status|.
  static int GetIdentityIconID(PageInfo::SiteIdentityStatus status);

  // Returns the connection icon ID for the given connection |status|.
  static int GetConnectionIconID(PageInfo::SiteConnectionStatus status);
#else
  // Returns the icon for the Certificate area.
  static const gfx::ImageSkia GetCertificateIcon();
#endif

  // Returns true if the Certificate Viewer link should be shown.
  static bool ShouldShowCertificateLink();

  // Return true if the given ContentSettingsType is in PageInfoUI.
  static bool ContentSettingsTypeInPageInfo(ContentSettingsType type);

  // Sets cookie information.
  virtual void SetCookieInfo(const CookieInfoList& cookie_info_list) = 0;

  // Sets permission information.
  virtual void SetPermissionInfo(
      const PermissionInfoList& permission_info_list,
      ChosenObjectInfoList chosen_object_info_list) = 0;

  // Sets site identity information.
  virtual void SetIdentityInfo(const IdentityInfo& identity_info) = 0;
};

typedef PageInfoUI::CookieInfoList CookieInfoList;
typedef PageInfoUI::PermissionInfoList PermissionInfoList;
typedef PageInfoUI::ChosenObjectInfoList ChosenObjectInfoList;

#endif  // CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_UI_H_
