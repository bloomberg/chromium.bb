// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/origin_chip.h"

#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/client_side_detection_host.h"
#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "grit/component_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// For selected kChromeUIScheme and kAboutScheme, return the string resource
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
#if defined(ENABLE_MDNS)
  if (host == chrome::kChromeUIDevicesHost)
    return IDS_LOCAL_DISCOVERY_DEVICES_PAGE_TITLE;
#endif  // ENABLE_MDNS
  if (host == chrome::kChromeUIDownloadsHost)
    return IDS_DOWNLOAD_TITLE;
  if (host == chrome::kChromeUIExtensionsHost)
    return IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE;
  if (host == chrome::kChromeUIHelpHost)
    return IDS_ABOUT_TAB_TITLE;
  if (host == chrome::kChromeUIHistoryHost)
    return IDS_HISTORY_TITLE;
  if (host == chrome::kChromeUINewTabHost)
    return IDS_NEW_TAB_TITLE;
  if (host == chrome::kChromeUIPluginsHost)
    return IDS_PLUGINS_TITLE;
  if (host == chrome::kChromeUIPolicyHost)
    return IDS_POLICY_TITLE;
  if (host == chrome::kChromeUIPrintHost)
    return IDS_PRINT_PREVIEW_TITLE;
  if (host == chrome::kChromeUISettingsHost)
    return IDS_SETTINGS_TITLE;
  if (host == chrome::kChromeUIVersionHost)
    return IDS_ABOUT_VERSION_TITLE;

  return -1;
}

}  // namespace

// static
bool OriginChip::IsMalware(const GURL& url, content::WebContents* tab) {
  if (tab->GetURL() != url)
    return false;

  safe_browsing::SafeBrowsingTabObserver* sb_observer =
      safe_browsing::SafeBrowsingTabObserver::FromWebContents(tab);
  return sb_observer && sb_observer->detection_host() &&
      sb_observer->detection_host()->DidPageReceiveSafeBrowsingMatch();
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
  if (url.SchemeIs(chrome::kAboutScheme))
    return base::UTF8ToUTF16(url.spec());

  // Chrome built-in pages.
  if (url.is_empty() || url.SchemeIs(content::kChromeUIScheme)) {
    int string_ref = StringForChromeHost(url);
    return (string_ref == -1) ?
        base::UTF8ToUTF16("Chrome") :
        l10n_util::GetStringUTF16(string_ref);
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

  if (url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(content::kFtpScheme)) {
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
  if (url.SchemeIs(chrome::kBlobScheme) ||
      url.SchemeIs(chrome::kChromeNativeScheme) ||
      url.SchemeIs(content::kChromeDevToolsScheme) ||
      url.SchemeIs(content::kDataScheme) ||
      url.SchemeIs(content::kFileScheme) ||
      url.SchemeIs(content::kFileSystemScheme) ||
      url.SchemeIs(content::kGuestScheme) ||
      url.SchemeIs(content::kJavaScriptScheme) ||
      url.SchemeIs(content::kMailToScheme) ||
      url.SchemeIs(content::kMetadataScheme) ||
      url.SchemeIs(content::kSwappedOutScheme)) {
    std::string truncated_url;
    base::TruncateUTF8ToByteSize(url.spec(), 1000, &truncated_url);
    return base::UTF8ToUTF16(truncated_url);
  }

#if defined(OS_CHROMEOS)
  if (url.SchemeIs(chrome::kCrosScheme) ||
      url.SchemeIs(chrome::kDriveScheme))
    return base::UTF8ToUTF16(url.spec());
#endif

  // If all else fails, return the hostname.
  return base::UTF8ToUTF16(url.host());
}
