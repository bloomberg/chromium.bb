// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/history/history_ui.h"

#include "base/command_line.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/grit/components_scaled_resources.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/search/search.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/history/web_history_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/ui/webui/history/browsing_history_handler.h"
#include "ios/chrome/browser/ui/webui/history/metrics_handler.h"
#include "ios/chrome/grit/ios_resources.h"
#include "ios/public/provider/web/web_ui_ios.h"
#include "ios/web/public/web_ui_ios_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

static const char kStringsJsFile[] = "strings.js";
static const char kHistoryJsFile[] = "history.js";
static const char kOtherDevicesJsFile[] = "other_devices.js";

namespace {

const char kIncognitoModeShortcut[] =
    "("
    "\xE2\x87\xA7"  // Shift symbol (U+21E7 'UPWARDS WHITE ARROW').
    "\xE2\x8C\x98"  // Command symbol (U+2318 'PLACE OF INTEREST SIGN').
    "N)";

web::WebUIIOSDataSource* CreateHistoryUIHTMLSource(
    ios::ChromeBrowserState* browser_state) {
  // Check if the browser state is authenticated.  Incognito tabs may not have
  // a sign in manager, and are considered not authenticated.
  SigninManagerBase* signin_manager =
      ios::SigninManagerFactory::GetForBrowserState(browser_state);
  bool is_authenticated =
      signin_manager != nullptr && signin_manager->IsAuthenticated();

  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIHistoryFrameHost);
  source->AddBoolean("isUserSignedIn", is_authenticated);
  source->AddLocalizedString("collapseSessionMenuItemText",
                             IDS_HISTORY_OTHER_SESSIONS_COLLAPSE_SESSION);
  source->AddLocalizedString("expandSessionMenuItemText",
                             IDS_HISTORY_OTHER_SESSIONS_EXPAND_SESSION);
  source->AddLocalizedString("restoreSessionMenuItemText",
                             IDS_HISTORY_OTHER_SESSIONS_OPEN_ALL);
  source->AddLocalizedString("xMore", IDS_HISTORY_OTHER_DEVICES_X_MORE);
  source->AddLocalizedString("loading", IDS_HISTORY_LOADING);
  source->AddLocalizedString("title", IDS_HISTORY_TITLE);
  source->AddLocalizedString("newest", IDS_HISTORY_NEWEST);
  source->AddLocalizedString("newer", IDS_HISTORY_NEWER);
  source->AddLocalizedString("older", IDS_HISTORY_OLDER);
  source->AddLocalizedString("searchResultsFor", IDS_HISTORY_SEARCHRESULTSFOR);
  source->AddLocalizedString("searchResult", IDS_HISTORY_SEARCH_RESULT);
  source->AddLocalizedString("searchResults", IDS_HISTORY_SEARCH_RESULTS);
  source->AddLocalizedString("foundSearchResults",
                             IDS_HISTORY_FOUND_SEARCH_RESULTS);
  source->AddLocalizedString("history", IDS_HISTORY_BROWSERESULTS);
  source->AddLocalizedString("cont", IDS_HISTORY_CONTINUED);
  source->AddLocalizedString("searchButton", IDS_HISTORY_SEARCH_BUTTON);
  source->AddLocalizedString("noSearchResults", IDS_HISTORY_NO_SEARCH_RESULTS);
  source->AddLocalizedString("noResults", IDS_HISTORY_NO_RESULTS);
  source->AddLocalizedString("historyInterval", IDS_HISTORY_INTERVAL);
  source->AddLocalizedString("removeSelected",
                             IDS_HISTORY_REMOVE_SELECTED_ITEMS);
  source->AddLocalizedString("clearAllHistory",
                             IDS_HISTORY_OPEN_CLEAR_BROWSING_DATA_DIALOG);

  base::string16 delete_warning =
      l10n_util::GetStringFUTF16(IDS_HISTORY_DELETE_PRIOR_VISITS_WARNING,
                                 base::UTF8ToUTF16(kIncognitoModeShortcut));
  source->AddString("deleteWarning", delete_warning);

  source->AddLocalizedString("removeBookmark", IDS_HISTORY_REMOVE_BOOKMARK);
  source->AddLocalizedString("actionMenuDescription",
                             IDS_HISTORY_ACTION_MENU_DESCRIPTION);
  source->AddLocalizedString("removeFromHistory", IDS_HISTORY_REMOVE_PAGE);
  source->AddLocalizedString("moreFromSite", IDS_HISTORY_MORE_FROM_SITE);
  source->AddLocalizedString("groupByDomainLabel",
                             IDS_HISTORY_GROUP_BY_DOMAIN_LABEL);
  source->AddLocalizedString("rangeLabel", IDS_HISTORY_RANGE_LABEL);
  source->AddLocalizedString("rangeAllTime", IDS_HISTORY_RANGE_ALL_TIME);
  source->AddLocalizedString("rangeWeek", IDS_HISTORY_RANGE_WEEK);
  source->AddLocalizedString("rangeMonth", IDS_HISTORY_RANGE_MONTH);
  source->AddLocalizedString("rangeToday", IDS_HISTORY_RANGE_TODAY);
  source->AddLocalizedString("rangeNext", IDS_HISTORY_RANGE_NEXT);
  source->AddLocalizedString("rangePrevious", IDS_HISTORY_RANGE_PREVIOUS);
  source->AddLocalizedString("numberVisits", IDS_HISTORY_NUMBER_VISITS);
  source->AddLocalizedString("filterAllowed", IDS_HISTORY_FILTER_ALLOWED);
  source->AddLocalizedString("filterBlocked", IDS_HISTORY_FILTER_BLOCKED);
  source->AddLocalizedString("inContentPack", IDS_HISTORY_IN_CONTENT_PACK);
  source->AddLocalizedString("allowItems", IDS_HISTORY_FILTER_ALLOW_ITEMS);
  source->AddLocalizedString("blockItems", IDS_HISTORY_FILTER_BLOCK_ITEMS);
  source->AddLocalizedString("blockedVisitText",
                             IDS_HISTORY_BLOCKED_VISIT_TEXT);
  source->AddLocalizedString("hasSyncedResults",
                             IDS_HISTORY_HAS_SYNCED_RESULTS);
  source->AddLocalizedString("noSyncedResults", IDS_HISTORY_NO_SYNCED_RESULTS);
  source->AddLocalizedString("cancel", IDS_CANCEL);
  source->AddLocalizedString("deleteConfirm",
                             IDS_HISTORY_DELETE_PRIOR_VISITS_CONFIRM_BUTTON);
  source->AddBoolean(
      "isFullHistorySyncEnabled",
      ios::WebHistoryServiceFactory::GetForBrowserState(browser_state) != NULL);
  bool group_by_domain = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kHistoryEnableGroupByDomain);
  source->AddBoolean("groupByDomain", group_by_domain);
  source->AddBoolean("allowDeletingHistory", true);
  source->AddBoolean("isInstantExtendedApiEnabled",
                     search::IsInstantExtendedAPIEnabled());
  source->AddBoolean("isSupervisedProfile", false);
  source->AddBoolean("hideDeleteVisitUI", false);
  source->SetJsonPath(kStringsJsFile);
  source->AddResourcePath(kHistoryJsFile, IDR_IOS_HISTORY_JS);
  source->AddResourcePath(kOtherDevicesJsFile,
                          IDR_IOS_HISTORY_OTHER_DEVICES_JS);
  source->SetDefaultResource(IDR_IOS_HISTORY_HTML);
  source->DisableDenyXFrameOptions();

  return source;
}

}  // namespace

HistoryUI::HistoryUI(web::WebUIIOS* web_ui) : web::WebUIIOSController(web_ui) {
  web_ui->AddMessageHandler(new BrowsingHistoryHandler());
  web_ui->AddMessageHandler(new MetricsHandler());

  // Set up the chrome://history-frame/ source.
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromWebUIIOS(web_ui);
  web::WebUIIOSDataSource::Add(browser_state,
                               CreateHistoryUIHTMLSource(browser_state));
}

HistoryUI::~HistoryUI() {}

// static
base::RefCountedMemory* HistoryUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_HISTORY_FAVICON, scale_factor);
}
