// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_ui.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/webui/browsing_history_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/grit/components_scaled_resources.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/foreign_session_handler.h"
#include "chrome/browser/ui/webui/history_login_handler.h"
#endif

static const char kStringsJsFile[] = "strings.js";
static const char kHistoryJsFile[] = "history.js";
static const char kOtherDevicesJsFile[] = "other_devices.js";

namespace {

static const char kMyActivityUrl[] =
    "https://history.google.com/history/?utm_source=chrome_h";

#if defined(OS_MACOSX)
const char kIncognitoModeShortcut[] = "("
    "\xE2\x87\xA7"  // Shift symbol (U+21E7 'UPWARDS WHITE ARROW').
    "\xE2\x8C\x98"  // Command symbol (U+2318 'PLACE OF INTEREST SIGN').
    "N)";
#elif defined(OS_WIN)
const char kIncognitoModeShortcut[] = "(Ctrl+Shift+N)";
#else
const char kIncognitoModeShortcut[] = "(Shift+Ctrl+N)";
#endif

constexpr char kIsUserSignedInKey[] = "isUserSignedIn";

bool IsSignedIn(Profile* profile) {
  // Check if the profile is authenticated.  Guest profiles or incognito
  // windows may not have a sign in manager, and are considered not
  // authenticated.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  return signin_manager && signin_manager->IsAuthenticated();
}

content::WebUIDataSource* CreateHistoryUIHTMLSource(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIHistoryFrameHost);
  source->AddBoolean(kIsUserSignedInKey, IsSignedIn(profile));
#if !defined(OS_ANDROID)
  source->AddLocalizedString("collapseSessionMenuItemText",
      IDS_HISTORY_OTHER_SESSIONS_COLLAPSE_SESSION);
  source->AddLocalizedString("expandSessionMenuItemText",
      IDS_HISTORY_OTHER_SESSIONS_EXPAND_SESSION);
  source->AddLocalizedString("restoreSessionMenuItemText",
      IDS_HISTORY_OTHER_SESSIONS_OPEN_ALL);
  source->AddLocalizedString("deleteSessionMenuItemText",
      IDS_HISTORY_OTHER_SESSIONS_HIDE_FOR_NOW);
#endif
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
  source->AddLocalizedString("removeSelected",
                             IDS_HISTORY_REMOVE_SELECTED_ITEMS);
  source->AddLocalizedString("clearAllHistory",
                             IDS_HISTORY_OPEN_CLEAR_BROWSING_DATA_DIALOG);

  auto availability = IncognitoModePrefs::GetAvailability(profile->GetPrefs());
  base::string16 delete_string = availability == IncognitoModePrefs::ENABLED
             ? l10n_util::GetStringFUTF16(
                   IDS_HISTORY_DELETE_PRIOR_VISITS_WARNING,
                   base::UTF8ToUTF16(kIncognitoModeShortcut))
             : l10n_util::GetStringUTF16(
                   IDS_HISTORY_DELETE_PRIOR_VISITS_WARNING_NO_INCOGNITO);
  source->AddString("deleteWarning", delete_string);

  source->AddLocalizedString("removeBookmark", IDS_HISTORY_REMOVE_BOOKMARK);
  source->AddLocalizedString("actionMenuDescription",
                             IDS_HISTORY_ACTION_MENU_DESCRIPTION);
  source->AddLocalizedString("removeFromHistory", IDS_HISTORY_REMOVE_PAGE);
  source->AddLocalizedString("moreFromSite", IDS_HISTORY_MORE_FROM_SITE);
  source->AddLocalizedString("rangeLabel", IDS_HISTORY_RANGE_LABEL);
  source->AddLocalizedString("rangeAllTime", IDS_HISTORY_RANGE_ALL_TIME);
  source->AddLocalizedString("rangeWeek", IDS_HISTORY_RANGE_WEEK);
  source->AddLocalizedString("rangeMonth", IDS_HISTORY_RANGE_MONTH);
  source->AddLocalizedString("rangeToday", IDS_HISTORY_RANGE_TODAY);
  source->AddLocalizedString("rangeNext", IDS_HISTORY_RANGE_NEXT);
  source->AddLocalizedString("rangePrevious", IDS_HISTORY_RANGE_PREVIOUS);
  source->AddLocalizedString("numberVisits", IDS_HISTORY_NUMBER_VISITS);
  source->AddLocalizedString("filterBlocked", IDS_HISTORY_FILTER_BLOCKED);
  source->AddLocalizedString("blockedVisitText",
                             IDS_HISTORY_BLOCKED_VISIT_TEXT);
  source->AddLocalizedString("hasSyncedResults",
                             IDS_HISTORY_HAS_SYNCED_RESULTS);
  source->AddLocalizedString("noSyncedResults", IDS_HISTORY_NO_SYNCED_RESULTS);
  source->AddString("otherFormsOfBrowsingHistory",
                    l10n_util::GetStringFUTF16(
                        IDS_HISTORY_OTHER_FORMS_OF_HISTORY,
                        base::ASCIIToUTF16(kMyActivityUrl)));
  source->AddLocalizedString("cancel", IDS_CANCEL);
  source->AddLocalizedString("deleteConfirm",
                             IDS_HISTORY_DELETE_PRIOR_VISITS_CONFIRM_BUTTON);
  source->AddLocalizedString("bookmarked", IDS_HISTORY_ENTRY_BOOKMARKED);
  source->AddLocalizedString("entrySummary", IDS_HISTORY_ENTRY_SUMMARY);
  bool group_by_domain = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kHistoryEnableGroupByDomain);
  // Supervised users get the "group by domain" version, but not on mobile,
  // because that version isn't adjusted for small screens yet. crbug.com/452859
#if !defined(OS_ANDROID)
  group_by_domain = group_by_domain || profile->IsSupervised();
#endif
  source->AddBoolean("groupByDomain", group_by_domain);
  bool allow_deleting_history =
      prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory);
  source->AddBoolean("allowDeletingHistory", allow_deleting_history);
  source->AddBoolean("isInstantExtendedApiEnabled",
                     search::IsInstantExtendedAPIEnabled());
  source->AddBoolean("isSupervisedProfile", profile->IsSupervised());
  source->AddBoolean("hideDeleteVisitUI",
                     profile->IsSupervised() && !allow_deleting_history);

  source->SetJsonPath(kStringsJsFile);
  source->AddResourcePath(kHistoryJsFile, IDR_HISTORY_JS);
  source->AddResourcePath(kOtherDevicesJsFile, IDR_OTHER_DEVICES_JS);
  source->SetDefaultResource(IDR_HISTORY_HTML);
  source->DisableDenyXFrameOptions();
  source->UseGzip(std::unordered_set<std::string>());

  return source;
}

}  // namespace

HistoryUI::HistoryUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://history-frame/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateHistoryUIHTMLSource(profile));

  web_ui->AddMessageHandler(base::MakeUnique<BrowsingHistoryHandler>());
  web_ui->AddMessageHandler(base::MakeUnique<MetricsHandler>());

  // On mobile we deal with foreign sessions differently.
#if !defined(OS_ANDROID)
  if (search::IsInstantExtendedAPIEnabled()) {
    web_ui->AddMessageHandler(
        base::MakeUnique<browser_sync::ForeignSessionHandler>());
    web_ui->AddMessageHandler(base::MakeUnique<HistoryLoginHandler>(
        base::Bind(&HistoryUI::UpdateDataSource, base::Unretained(this))));
  }
#endif

  // TODO(crbug.com/595332): Since the API to query other forms of browsing
  // history is not ready yet, make it possible to test the history UI as if
  // it were. If the user opens chrome://history/?reset_ofbh, we will assume
  // that other forms of browsing history exist (for all accounts), and we will
  // also reset the one-time notice shown in the Clear Browsing Data dialog.
  // This code should be removed as soon as the API is ready.
  GURL url = web_ui->GetWebContents()->GetVisibleURL();
  if (url.has_query() && url.query() == "reset_ofbh") {
    profile->GetPrefs()->SetInteger(
        browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes, 0);
    browsing_data::testing::
        g_override_other_forms_of_browsing_history_query = true;
  }
}

HistoryUI::~HistoryUI() {}

// static
base::RefCountedMemory* HistoryUI::GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor) {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytesForScale(IDR_HISTORY_FAVICON, scale_factor);
}

void HistoryUI::UpdateDataSource() {
  CHECK(web_ui());
  Profile* profile = Profile::FromWebUI(web_ui());
  std::unique_ptr<base::DictionaryValue> update(new base::DictionaryValue);
  update->SetBoolean(kIsUserSignedInKey, IsSignedIn(profile));
  content::WebUIDataSource::Update(profile, chrome::kChromeUIHistoryFrameHost,
                                   std::move(update));
}
