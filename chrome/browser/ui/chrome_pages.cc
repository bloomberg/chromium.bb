// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_pages.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/settings_window_manager.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/options/content_settings_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "ui/base/window_open_disposition.h"

#if defined(OS_WIN)
#include "chrome/browser/enumerate_modules_model_win.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/genius_app/app_id.h"
#include "extensions/browser/extension_registry.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/signin_manager.h"
#endif

using base::UserMetricsAction;

namespace chrome {
namespace {

const char kHashMark[] = "#";

void OpenBookmarkManagerWithHash(Browser* browser,
                                 const std::string& action,
                                 int64_t node_id) {
  content::RecordAction(UserMetricsAction("ShowBookmarkManager"));
  content::RecordAction(UserMetricsAction("ShowBookmarks"));
  NavigateParams params(GetSingletonTabNavigateParams(
      browser,
      GURL(kChromeUIBookmarksURL).Resolve(base::StringPrintf(
          "/#%s%s", action.c_str(), base::Int64ToString(node_id).c_str()))));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, params);
}

void NavigateToSingletonTab(Browser* browser, const GURL& url) {
  NavigateParams params(GetSingletonTabNavigateParams(browser, url));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, params);
}

// Shows either the help app or the appropriate help page for |source|. If
// |browser| is NULL and the help page is used (vs the app), the help page is
// shown in the last active browser. If there is no such browser, a new browser
// is created.
void ShowHelpImpl(Browser* browser,
                  Profile* profile,
                  HostDesktopType host_desktop_type,
                  HelpSource source) {
  content::RecordAction(UserMetricsAction("ShowHelpTab"));
#if defined(OS_CHROMEOS) && defined(OFFICIAL_BUILD)
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          genius_app::kGeniusAppId,
          extensions::ExtensionRegistry::EVERYTHING);
  extensions::AppLaunchSource app_launch_source(extensions::SOURCE_UNTRACKED);
  switch (source) {
    case HELP_SOURCE_KEYBOARD:
      app_launch_source = extensions::SOURCE_KEYBOARD;
      break;
    case HELP_SOURCE_MENU:
      app_launch_source = extensions::SOURCE_SYSTEM_TRAY;
      break;
    case HELP_SOURCE_WEBUI:
      app_launch_source = extensions::SOURCE_ABOUT_PAGE;
      break;
    default:
      NOTREACHED() << "Unhandled help source" << source;
  }
  AppLaunchParams params(profile, extension, CURRENT_TAB, host_desktop_type,
                         app_launch_source);
  OpenApplication(params);
#else
  GURL url;
  switch (source) {
    case HELP_SOURCE_KEYBOARD:
      url = GURL(kChromeHelpViaKeyboardURL);
      break;
    case HELP_SOURCE_MENU:
      url = GURL(kChromeHelpViaMenuURL);
      break;
    case HELP_SOURCE_WEBUI:
      url = GURL(kChromeHelpViaWebUIURL);
      break;
    default:
      NOTREACHED() << "Unhandled help source " << source;
  }
  scoped_ptr<ScopedTabbedBrowserDisplayer> displayer;
  if (!browser) {
    displayer.reset(new ScopedTabbedBrowserDisplayer(profile));
    browser = displayer->browser();
  }
  ShowSingletonTab(browser, url);
#endif
}

std::string GenerateContentSettingsExceptionsSubPage(ContentSettingsType type) {
  return kContentSettingsExceptionsSubPage + std::string(kHashMark) +
         options::ContentSettingsHandler::ContentSettingsTypeToGroupName(type);
}

}  // namespace

void ShowBookmarkManager(Browser* browser) {
  content::RecordAction(UserMetricsAction("ShowBookmarkManager"));
  content::RecordAction(UserMetricsAction("ShowBookmarks"));
  ShowSingletonTabOverwritingNTP(
      browser,
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIBookmarksURL)));
}

void ShowBookmarkManagerForNode(Browser* browser, int64_t node_id) {
  OpenBookmarkManagerWithHash(browser, std::string(), node_id);
}

void ShowHistory(Browser* browser) {
  content::RecordAction(UserMetricsAction("ShowHistory"));
  NavigateParams params(
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIHistoryURL)));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, params);
}

void ShowDownloads(Browser* browser) {
  content::RecordAction(UserMetricsAction("ShowDownloads"));
  if (browser->window()) {
    DownloadShelf* shelf = browser->window()->GetDownloadShelf();
    // The downloads page is always shown in response to a user action.
    if (shelf->IsShowing())
      shelf->Close(DownloadShelf::USER_ACTION);
  }
  ShowSingletonTabOverwritingNTP(
      browser,
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIDownloadsURL)));
}

void ShowExtensions(Browser* browser,
                    const std::string& extension_to_highlight) {
  content::RecordAction(UserMetricsAction("ShowExtensions"));
  NavigateParams params(
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIExtensionsURL)));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  if (!extension_to_highlight.empty()) {
    GURL::Replacements replacements;
    std::string query("id=");
    query += extension_to_highlight;
    replacements.SetQueryStr(query);
    params.url = params.url.ReplaceComponents(replacements);
  }
  ShowSingletonTabOverwritingNTP(browser, params);
}

void ShowConflicts(Browser* browser) {
#if defined(OS_WIN)
  EnumerateModulesModel* model = EnumerateModulesModel::GetInstance();
  if (model->modules_to_notify_about() > 0) {
    GURL help_center_url = model->GetFirstNotableConflict();
    if (help_center_url.is_valid()) {
      ShowSingletonTab(browser, help_center_url);
      model->AcknowledgeConflictNotification();
      return;
    }
  }
#endif

  content::RecordAction(UserMetricsAction("AboutConflicts"));
  ShowSingletonTab(browser, GURL(kChromeUIConflictsURL));
}

void ShowHelp(Browser* browser, HelpSource source) {
  ShowHelpImpl(
      browser, browser->profile(), browser->host_desktop_type(), source);
}

void ShowHelpForProfile(Profile* profile,
                        HostDesktopType host_desktop_type,
                        HelpSource source) {
  ShowHelpImpl(NULL, profile, host_desktop_type, source);
}

void ShowPolicy(Browser* browser) {
  ShowSingletonTab(browser, GURL(kChromeUIPolicyURL));
}

void ShowSlow(Browser* browser) {
#if defined(OS_CHROMEOS)
  ShowSingletonTab(browser, GURL(kChromeUISlowURL));
#endif
}

void ShowMemory(Browser* browser) {
  ShowSingletonTab(browser, GURL(kChromeUIMemoryURL));
}

GURL GetSettingsUrl(const std::string& sub_page) {
  std::string url = std::string(kChromeUISettingsURL) + sub_page;
#if defined(OS_CHROMEOS)
  if (sub_page.find(kInternetOptionsSubPage, 0) != std::string::npos) {
    std::string::size_type loc = sub_page.find("?", 0);
    std::string network_page =
        loc != std::string::npos ? sub_page.substr(loc) : std::string();
    url = std::string(kChromeUISettingsURL) + network_page;
  }
#endif
  return GURL(url);
}

bool IsSettingsSubPage(const GURL& url, const std::string& sub_page) {
  return (url.SchemeIs(content::kChromeUIScheme) &&
          (url.host_piece() == chrome::kChromeUISettingsHost ||
           url.host_piece() == chrome::kChromeUISettingsFrameHost) &&
          url.path_piece() == "/" + sub_page);
}

bool IsTrustedPopupWindowWithScheme(const Browser* browser,
                                    const std::string& scheme) {
  if (!browser->is_type_popup() || !browser->is_trusted_source())
    return false;
  if (scheme.empty())  // Any trusted popup window
    return true;
  const content::WebContents* web_contents =
      browser->tab_strip_model()->GetWebContentsAt(0);
  if (!web_contents)
    return false;
  GURL url(web_contents->GetURL());
  return url.SchemeIs(scheme.c_str());
}


void ShowSettings(Browser* browser) {
  ShowSettingsSubPage(browser, std::string());
}

void ShowSettingsSubPage(Browser* browser, const std::string& sub_page) {
  if (::switches::SettingsWindowEnabled()) {
    ShowSettingsSubPageForProfile(browser->profile(), sub_page);
    return;
  }
  ShowSettingsSubPageInTabbedBrowser(browser, sub_page);
}

void ShowSettingsSubPageForProfile(Profile* profile,
                                   const std::string& sub_page) {
  if (::switches::SettingsWindowEnabled()) {
    content::RecordAction(base::UserMetricsAction("ShowOptions"));
    SettingsWindowManager::GetInstance()->ShowChromePageForProfile(
        profile, GetSettingsUrl(sub_page));
    return;
  }
  Browser* browser = chrome::FindTabbedBrowser(profile, false);
  if (!browser) {
    browser = new Browser(Browser::CreateParams(profile));
  }
  ShowSettingsSubPageInTabbedBrowser(browser, sub_page);
}

void ShowSettingsSubPageInTabbedBrowser(Browser* browser,
                                        const std::string& sub_page) {
  content::RecordAction(UserMetricsAction("ShowOptions"));
  GURL gurl = GetSettingsUrl(sub_page);
  NavigateParams params(GetSingletonTabNavigateParams(browser, gurl));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, params);
}

void ShowContentSettingsExceptions(Browser* browser,
                                   ContentSettingsType content_settings_type) {
  ShowSettingsSubPage(
      browser, GenerateContentSettingsExceptionsSubPage(content_settings_type));
}

void ShowContentSettingsExceptionsInWindow(
    Profile* profile,
    ContentSettingsType content_settings_type) {
  DCHECK(switches::SettingsWindowEnabled());
  ShowSettingsSubPageForProfile(
      profile, GenerateContentSettingsExceptionsSubPage(content_settings_type));
}

void ShowContentSettings(Browser* browser,
                         ContentSettingsType content_settings_type) {
  ShowSettingsSubPage(
      browser,
      kContentSettingsSubPage + std::string(kHashMark) +
          options::ContentSettingsHandler::ContentSettingsTypeToGroupName(
              content_settings_type));
}

void ShowClearBrowsingDataDialog(Browser* browser) {
  content::RecordAction(UserMetricsAction("ClearBrowsingData_ShowDlg"));
  ShowSettingsSubPage(browser, kClearBrowserDataSubPage);
}

void ShowPasswordManager(Browser* browser) {
  content::RecordAction(UserMetricsAction("Options_ShowPasswordManager"));
  ShowSettingsSubPage(browser, kPasswordManagerSubPage);
}

void ShowImportDialog(Browser* browser) {
  content::RecordAction(UserMetricsAction("Import_ShowDlg"));
  ShowSettingsSubPage(browser, kImportDataSubPage);
}

void ShowAboutChrome(Browser* browser) {
  content::RecordAction(UserMetricsAction("AboutChrome"));
  if (::switches::SettingsWindowEnabled()) {
    SettingsWindowManager::GetInstance()->ShowChromePageForProfile(
        browser->profile(), GURL(kChromeUIHelpURL));
    return;
  }
  NavigateParams params(
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIHelpURL)));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, params);
}

void ShowSearchEngineSettings(Browser* browser) {
  content::RecordAction(UserMetricsAction("EditSearchEngines"));
  ShowSettingsSubPage(browser, kSearchEnginesSubPage);
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
void ShowBrowserSignin(Browser* browser,
                       signin_metrics::AccessPoint access_point) {
  Profile* original_profile = browser->profile()->GetOriginalProfile();
  SigninManagerBase* manager =
      SigninManagerFactory::GetForProfile(original_profile);
  DCHECK(manager->IsSigninAllowed());
  // If the browser's profile is an incognito profile, make sure to use
  // a browser window from the original profile.  The user cannot sign in
  // from an incognito window.
  bool switched_browser = false;
  scoped_ptr<ScopedTabbedBrowserDisplayer> displayer;
  if (browser->profile()->IsOffTheRecord()) {
    switched_browser = true;
    displayer.reset(new ScopedTabbedBrowserDisplayer(original_profile));
    browser = displayer->browser();
  }

  // Since the extension is a separate application, it might steal focus
  // away from Chrome, and accidentally close the avatar bubble. The same will
  // happen if we had to switch browser windows to show the sign in page. In
  // this case, fallback to the full-tab signin page.
  bool show_avatar_bubble =
      access_point != signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS &&
      !switched_browser;
#if defined(OS_CHROMEOS)
  // ChromeOS doesn't have the avatar bubble.
  show_avatar_bubble = false;
#endif

  if (show_avatar_bubble) {
    browser->window()->ShowAvatarBubbleFromAvatarButton(
        BrowserWindow::AVATAR_BUBBLE_MODE_SIGNIN,
        signin::ManageAccountsParams(), access_point);
  } else {
    NavigateToSingletonTab(
        browser,
        signin::GetPromoURL(
            access_point, signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT,
            false));
    DCHECK_GT(browser->tab_strip_model()->count(), 0);
  }
}

void ShowBrowserSigninOrSettings(Browser* browser,
                                 signin_metrics::AccessPoint access_point) {
  Profile* original_profile = browser->profile()->GetOriginalProfile();
  SigninManagerBase* manager =
      SigninManagerFactory::GetForProfile(original_profile);
  DCHECK(manager->IsSigninAllowed());
  if (manager->IsAuthenticated())
    ShowSettings(browser);
  else
    ShowBrowserSignin(browser, access_point);
}
#endif

}  // namespace chrome
