// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/location_bar/origin_chip_info.h"

#include <string>

#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/client_side_detection_host.h"
#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// For selected kChromeUIScheme and url::kAboutScheme, return the string
// resource
// number for the title of the page. If we don't have a specialized title,
// returns -1.
int StringForChromeHost(const GURL& url) {
  DCHECK(url.is_empty() || url.SchemeIs(content::kChromeUIScheme));

  if (url.is_empty())
    return IDS_NEW_TAB_TITLE;

  // TODO(gbillock): Just get the page title and special case exceptions?
  std::string host = url.host();
  if (host == chrome::kChromeUIAppLauncherPageHost)
    return IDS_APP_DEFAULT_PAGE_NAME;
  if (host == chrome::kChromeUIBookmarksHost)
    return IDS_BOOKMARK_MANAGER_TITLE;
  if (host == chrome::kChromeUIComponentsHost)
    return IDS_COMPONENTS_TITLE;
  if (host == chrome::kChromeUICrashesHost)
    return IDS_CRASHES_TITLE;
#if defined(ENABLE_SERVICE_DISCOVERY)
  if (host == chrome::kChromeUIDevicesHost)
    return IDS_LOCAL_DISCOVERY_DEVICES_PAGE_TITLE;
#endif  // ENABLE_SERVICE_DISCOVERY
  if (host == chrome::kChromeUIDownloadsHost)
    return IDS_DOWNLOAD_TITLE;
  if (host == chrome::kChromeUIExtensionsHost)
    return IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE;
  if (host == chrome::kChromeUIHelpHost)
    return IDS_ABOUT_TITLE;
  if (host == chrome::kChromeUIHistoryHost)
    return IDS_HISTORY_TITLE;
  if (host == chrome::kChromeUINewTabHost)
    return IDS_NEW_TAB_TITLE;
  if (host == chrome::kChromeUIPluginsHost)
    return IDS_PLUGINS_TITLE;
  if (host == chrome::kChromeUIPolicyHost)
    return IDS_POLICY_TITLE;
#if defined(ENABLE_FULL_PRINTING)
  if (host == chrome::kChromeUIPrintHost)
    return IDS_PRINT_PREVIEW_TITLE;
#endif  // ENABLE_FULL_PRINTING
  if (host == chrome::kChromeUISettingsHost)
    return IDS_SETTINGS_TITLE;
  if (host == chrome::kChromeUIVersionHost)
    return IDS_ABOUT_VERSION_TITLE;

  return -1;
}

}  // namespace

OriginChipInfo::OriginChipInfo(extensions::IconImage::Observer* owner,
                               Profile* profile)
    : owner_(owner),
      profile_(profile),
      security_level_(ToolbarModel::NONE),
      is_url_malware_(false),
      icon_(IDR_PRODUCT_LOGO_16) {
}

OriginChipInfo::~OriginChipInfo() {
}

bool OriginChipInfo::Update(const content::WebContents* web_contents,
                            const ToolbarModel* toolbar_model) {
  GURL displayed_url = toolbar_model->GetURL();
  ToolbarModel::SecurityLevel security_level =
      toolbar_model->GetSecurityLevel(true);
  bool is_url_malware = OriginChip::IsMalware(displayed_url, web_contents);

  if ((displayed_url_ == displayed_url) &&
      (security_level_ == security_level) &&
      (is_url_malware_ == is_url_malware))
    return false;

  displayed_url_ = displayed_url;
  security_level_ = security_level;
  is_url_malware_ = is_url_malware;

  label_ = OriginChip::LabelFromURLForProfile(displayed_url, profile_);
  if (security_level_ == ToolbarModel::EV_SECURE) {
    label_ = l10n_util::GetStringFUTF16(IDS_SITE_CHIP_EV_SSL_LABEL,
                                        toolbar_model->GetEVCertName(),
                                        label_);
  }


  if (displayed_url_.SchemeIs(extensions::kExtensionScheme)) {
    const extensions::Extension* extension =
        extensions::ExtensionSystem::Get(profile_)->extension_service()->
            extensions()->GetExtensionOrAppByURL(displayed_url_);

    if (extension) {
      icon_ = IDR_EXTENSIONS_FAVICON;
      extension_icon_image_.reset(
          new extensions::IconImage(profile_,
                                    extension,
                                    extensions::IconsInfo::GetIcons(extension),
                                    extension_misc::EXTENSION_ICON_BITTY,
                                    extensions::util::GetDefaultAppIcon(),
                                    owner_));

      // Forces load of the image.
      extension_icon_image_->image_skia().GetRepresentation(1.0f);
      if (!extension_icon_image_->image_skia().image_reps().empty())
        owner_->OnExtensionIconImageChanged(extension_icon_image_.get());

      return true;
    }
  }

  if (extension_icon_image_) {
    extension_icon_image_.reset();
    owner_->OnExtensionIconImageChanged(NULL);
  }

  icon_ = (displayed_url_.is_empty() ||
           displayed_url_.SchemeIs(content::kChromeUIScheme)) ?
      IDR_PRODUCT_LOGO_16 :
      toolbar_model->GetIconForSecurityLevel(security_level_);

  return true;
}

base::string16 OriginChipInfo::Tooltip() const {
  return base::UTF8ToUTF16(displayed_url_.spec());
}

// static
bool OriginChip::IsMalware(const GURL& url, const content::WebContents* tab) {
  DCHECK(tab);

  if (tab->GetURL() != url)
    return false;

  const safe_browsing::SafeBrowsingTabObserver* sb_observer =
      safe_browsing::SafeBrowsingTabObserver::FromWebContents(tab);
  return sb_observer && sb_observer->DidPageReceiveSafeBrowsingMatch();
}

// static
base::string16 OriginChip::LabelFromURLForProfile(const GURL& provided_url,
                                                  Profile* profile) {
  // First, strip view-source: if it appears.  Note that GetContent removes
  // "view-source:" but leaves the original scheme (http, https, ftp, etc).
  GURL url(provided_url);
  if (url.SchemeIs(content::kViewSourceScheme))
    url = GURL(url.GetContent());

  // About scheme pages. Currently all about: URLs other than about:blank
  // redirect to chrome: URLs, so this only affects about:blank.
  if (url.SchemeIs(url::kAboutScheme))
    return base::UTF8ToUTF16(url.spec());

  // Chrome built-in pages.
  if (url.is_empty() || url.SchemeIs(content::kChromeUIScheme)) {
    int string_ref = StringForChromeHost(url);
    return l10n_util::GetStringUTF16(
        (string_ref == -1) ? IDS_SHORT_PRODUCT_NAME : string_ref);
  }

  // For chrome-extension URLs, return the extension name.
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    ExtensionService* service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    const extensions::Extension* extension =
        service->extensions()->GetExtensionOrAppByURL(url);
    return extension ?
        base::UTF8ToUTF16(extension->name()) : base::UTF8ToUTF16(url.host());
  }

  if (url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(url::kFtpScheme)) {
    // See ToolbarModelImpl::GetText(). Does not pay attention to any user
    // edits, and uses GetURL/net::FormatUrl -- We don't really care about
    // length or the autocomplete parser.
    // TODO(gbillock): This uses an algorithm very similar to GetText, which
    // is probably too conservative. Try out just using a simpler mechanism of
    // StripWWW() and IDNToUnicode().
    std::string languages;
    if (profile)
      languages = profile->GetPrefs()->GetString(prefs::kAcceptLanguages);

    // TODO(macourteau): Extract the bits we care about from FormatUrl to
    // format |url.host()| instead of this.
    base::string16 formatted = net::FormatUrl(url.GetOrigin(), languages,
        net::kFormatUrlOmitAll, net::UnescapeRule::NORMAL, NULL, NULL, NULL);
    // Remove scheme, "www.", and trailing "/".
    if (StartsWith(formatted, base::ASCIIToUTF16("http://"), false))
      formatted = formatted.substr(7);
    else if (StartsWith(formatted, base::ASCIIToUTF16("https://"), false))
      formatted = formatted.substr(8);
    else if (StartsWith(formatted, base::ASCIIToUTF16("ftp://"), false))
      formatted = formatted.substr(6);
    if (StartsWith(formatted, base::ASCIIToUTF16("www."), false))
      formatted = formatted.substr(4);
    if (EndsWith(formatted, base::ASCIIToUTF16("/"), false))
      formatted = formatted.substr(0, formatted.size() - 1);
    return formatted;
  }

  // These internal-ish debugging-style schemes we don't expect users
  // to see. In these cases, the site chip will display the first
  // part of the full URL.
  if (url.SchemeIs(chrome::kChromeNativeScheme) ||
      url.SchemeIs(url::kBlobScheme) ||
      url.SchemeIs(content::kChromeDevToolsScheme) ||
      url.SchemeIs(url::kDataScheme) ||
      url.SchemeIs(url::kFileScheme) ||
      url.SchemeIs(url::kFileSystemScheme) ||
      url.SchemeIs(content::kGuestScheme) ||
      url.SchemeIs(url::kJavaScriptScheme) ||
      url.SchemeIs(url::kMailToScheme) ||
      url.SchemeIs(content::kMetadataScheme) ||
      url.SchemeIs(content::kSwappedOutScheme)) {
    std::string truncated_url;
    base::TruncateUTF8ToByteSize(url.spec(), 1000, &truncated_url);
    return base::UTF8ToUTF16(truncated_url);
  }

#if defined(OS_CHROMEOS)
  if (url.SchemeIs(chrome::kCrosScheme) ||
      url.SchemeIs(chrome::kExternalFileScheme))
    return base::UTF8ToUTF16(url.spec());
#endif

  // If all else fails, return the hostname.
  return base::UTF8ToUTF16(url.host());
}
