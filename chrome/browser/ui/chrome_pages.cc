// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_pages.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/webui/options2/content_settings_handler2.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"

using content::UserMetricsAction;

namespace chrome {
namespace {

const char kHashMark[] = "#";

void OpenBookmarkManagerWithHash(Browser* browser,
                                 const std::string& action,
                                 int64 node_id) {
  content::RecordAction(UserMetricsAction("ShowBookmarkManager"));
  content::RecordAction(UserMetricsAction("ShowBookmarks"));
  browser::NavigateParams params(GetSingletonTabNavigateParams(
      browser,
      GURL(kChromeUIBookmarksURL).Resolve(
      StringPrintf("/#%s%s", action.c_str(),
      base::Int64ToString(node_id).c_str()))));
  params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, params);
}

}  // namespace

void ShowBookmarkManager(Browser* browser) {
  content::RecordAction(UserMetricsAction("ShowBookmarkManager"));
  content::RecordAction(UserMetricsAction("ShowBookmarks"));
  ShowSingletonTabOverwritingNTP(
      browser,
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIBookmarksURL)));
}

void ShowBookmarkManagerForNode(Browser* browser, int64 node_id) {
  OpenBookmarkManagerWithHash(browser, "", node_id);
}

void ShowHistory(Browser* browser) {
  content::RecordAction(UserMetricsAction("ShowHistory"));
  browser::NavigateParams params(
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIHistoryURL)));
  params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, params);
}

void ShowDownloads(Browser* browser) {
  content::RecordAction(UserMetricsAction("ShowDownloads"));
  if (browser->window()) {
    DownloadShelf* shelf = browser->window()->GetDownloadShelf();
    if (shelf->IsShowing())
      shelf->Close();
  }
  ShowSingletonTabOverwritingNTP(
      browser,
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIDownloadsURL)));
}

void ShowExtensions(Browser* browser) {
  content::RecordAction(UserMetricsAction("ShowExtensions"));
  browser::NavigateParams params(
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIExtensionsURL)));
  params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, params);
}

void ShowConflicts(Browser* browser) {
  content::RecordAction(UserMetricsAction("AboutConflicts"));
  ShowSingletonTab(browser, GURL(kChromeUIConflictsURL));
}

void ShowHelp(Browser* browser, HelpSource source) {
  content::RecordAction(UserMetricsAction("ShowHelpTab"));
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
  ShowSingletonTab(browser, url);
}

void ShowSettings(Browser* browser) {
  content::RecordAction(UserMetricsAction("ShowOptions"));
  ShowSettingsSubPage(browser, std::string());
}

void ShowSettingsSubPage(Browser* browser, const std::string& sub_page) {
  std::string url = std::string(kChromeUISettingsURL) + sub_page;
#if defined(OS_CHROMEOS)
  if (sub_page.find(kInternetOptionsSubPage, 0) != std::string::npos) {
    std::string::size_type loc = sub_page.find("?", 0);
    std::string network_page = loc != std::string::npos ?
        sub_page.substr(loc) : std::string();
    url = std::string(kChromeUISettingsURL) + network_page;
  }
#endif
  browser::NavigateParams params(
      GetSingletonTabNavigateParams(browser, GURL(url)));
  params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, params);
}

void ShowContentSettings(Browser* browser,
                         ContentSettingsType content_settings_type) {
  ShowSettingsSubPage(
      browser,
      kContentSettingsExceptionsSubPage + std::string(kHashMark) +
      options2::ContentSettingsHandler::ContentSettingsTypeToGroupName(
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

void ShowInstantConfirmDialog(Browser* browser) {
  ShowSettingsSubPage(browser, kInstantConfirmPage);
}

void ShowAboutChrome(Browser* browser) {
  content::RecordAction(UserMetricsAction("AboutChrome"));
#if !defined(OS_WIN)
  browser::NavigateParams params(
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIUberURL)));
  params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, params);
#else
  // crbug.com/115123.
  browser->window()->ShowAboutChromeDialog();
#endif
}

void ShowSearchEngineSettings(Browser* browser) {
  content::RecordAction(UserMetricsAction("EditSearchEngines"));
  ShowSettingsSubPage(browser, kSearchEnginesSubPage);
}

void ShowSyncSetup(Browser* browser, SyncPromoUI::Source source) {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(
          browser->profile()->GetOriginalProfile());
  LoginUIService* login_service = LoginUIServiceFactory::GetForProfile(
      browser->profile()->GetOriginalProfile());
  if (service->HasSyncSetupCompleted()) {
    ShowSettings(browser);
  } else if (SyncPromoUI::ShouldShowSyncPromo(browser->profile()) &&
             login_service->current_login_ui() == NULL) {
    // There is no currently active login UI, so display a new promo page.
    GURL url(SyncPromoUI::GetSyncPromoURL(GURL(), source));
    browser::NavigateParams params(
        GetSingletonTabNavigateParams(browser, GURL(url)));
    params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
    ShowSingletonTabOverwritingNTP(browser, params);
  } else {
    LoginUIServiceFactory::GetForProfile(
        browser->profile()->GetOriginalProfile())->ShowLoginUI(browser);
  }
}

}  // namespace chrome
