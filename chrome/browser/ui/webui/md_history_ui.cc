// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_history_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/webui/browsing_history_handler.h"
#include "chrome/browser/ui/webui/foreign_session_handler.h"
#include "chrome/browser/ui/webui/history_login_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "chrome/grit/theme_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

content::WebUIDataSource* CreateMdHistoryUIHTMLSource(Profile* profile,
                                                      bool use_test_title) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIHistoryHost);

  // Localized strings (alphabetical order).
  source->AddLocalizedString("bookmarked", IDS_HISTORY_ENTRY_BOOKMARKED);
  source->AddLocalizedString("cancel", IDS_CANCEL);
  source->AddLocalizedString("clearBrowsingData",
                             IDS_CLEAR_BROWSING_DATA_TITLE);
  source->AddLocalizedString("clearSearch", IDS_MD_HISTORY_CLEAR_SEARCH);
  source->AddLocalizedString("closeMenuPromo", IDS_MD_HISTORY_CLOSE_MENU_PROMO);
  source->AddLocalizedString("collapseSessionButton",
                             IDS_HISTORY_OTHER_SESSIONS_COLLAPSE_SESSION);
  source->AddLocalizedString("delete", IDS_MD_HISTORY_DELETE);
  source->AddLocalizedString("deleteConfirm",
                             IDS_HISTORY_DELETE_PRIOR_VISITS_CONFIRM_BUTTON);
  source->AddLocalizedString("deleteSession",
                             IDS_HISTORY_OTHER_SESSIONS_HIDE_FOR_NOW);
  source->AddLocalizedString(
      "deleteWarning", IDS_HISTORY_DELETE_PRIOR_VISITS_WARNING_NO_INCOGNITO);
  source->AddLocalizedString("entrySummary", IDS_HISTORY_ENTRY_SUMMARY);
  source->AddLocalizedString("expandSessionButton",
                             IDS_HISTORY_OTHER_SESSIONS_EXPAND_SESSION);
  source->AddLocalizedString("foundSearchResults",
                             IDS_HISTORY_FOUND_SEARCH_RESULTS);
  source->AddLocalizedString("hasSyncedResults",
                             IDS_MD_HISTORY_HAS_SYNCED_RESULTS);
  source->AddLocalizedString("hasSyncedResultsDescription",
                             IDS_MD_HISTORY_HAS_SYNCED_RESULTS_DESCRIPTION);
  source->AddLocalizedString("historyInterval", IDS_HISTORY_INTERVAL);
  source->AddLocalizedString("historyMenuButton",
                             IDS_MD_HISTORY_HISTORY_MENU_DESCRIPTION);
  source->AddLocalizedString("historyMenuItem",
                             IDS_MD_HISTORY_HISTORY_MENU_ITEM);
  source->AddLocalizedString("itemsSelected", IDS_MD_HISTORY_ITEMS_SELECTED);
  source->AddLocalizedString("loading", IDS_HISTORY_LOADING);
  source->AddLocalizedString("menuPromo", IDS_MD_HISTORY_MENU_PROMO);
  source->AddLocalizedString("moreActionsButton",
                             IDS_HISTORY_ACTION_MENU_DESCRIPTION);
  source->AddLocalizedString("moreFromSite", IDS_HISTORY_MORE_FROM_SITE);
  source->AddLocalizedString("openAll", IDS_HISTORY_OTHER_SESSIONS_OPEN_ALL);
  source->AddLocalizedString("openTabsMenuItem",
                             IDS_MD_HISTORY_OPEN_TABS_MENU_ITEM);
  source->AddLocalizedString("noResults", IDS_HISTORY_NO_RESULTS);
  source->AddLocalizedString("noSearchResults", IDS_HISTORY_NO_SEARCH_RESULTS);
  source->AddLocalizedString("noSyncedResults",
                             IDS_MD_HISTORY_NO_SYNCED_RESULTS);
  source->AddLocalizedString("rangeAllTime", IDS_HISTORY_RANGE_ALL_TIME);
  source->AddLocalizedString("rangeWeek", IDS_HISTORY_RANGE_WEEK);
  source->AddLocalizedString("rangeMonth", IDS_HISTORY_RANGE_MONTH);
  source->AddLocalizedString("rangeToday", IDS_HISTORY_RANGE_TODAY);
  source->AddLocalizedString("rangeNext", IDS_HISTORY_RANGE_NEXT);
  source->AddLocalizedString("rangePrevious", IDS_HISTORY_RANGE_PREVIOUS);
  source->AddLocalizedString("removeBookmark", IDS_HISTORY_REMOVE_BOOKMARK);
  source->AddLocalizedString("removeFromHistory", IDS_HISTORY_REMOVE_PAGE);
  source->AddLocalizedString("removeSelected",
                             IDS_HISTORY_REMOVE_SELECTED_ITEMS);
  source->AddLocalizedString("searchPrompt", IDS_MD_HISTORY_SEARCH_PROMPT);
  source->AddLocalizedString("searchResult", IDS_HISTORY_SEARCH_RESULT);
  source->AddLocalizedString("searchResults", IDS_HISTORY_SEARCH_RESULTS);
  source->AddLocalizedString("signInButton", IDS_MD_HISTORY_SIGN_IN_BUTTON);
  source->AddLocalizedString("signInPromo", IDS_MD_HISTORY_SIGN_IN_PROMO);
  source->AddLocalizedString("signInPromoDesc",
                             IDS_MD_HISTORY_SIGN_IN_PROMO_DESC);
  if (use_test_title)
    source->AddString("title", "MD History");
  else
    source->AddLocalizedString("title", IDS_HISTORY_TITLE);

  source->AddString(
      "sidebarFooter",
      l10n_util::GetStringFUTF16(
          IDS_HISTORY_OTHER_FORMS_OF_HISTORY,
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_CLEAR_DATA_WEB_HISTORY_URL_IN_HISTORY)));

  PrefService* prefs = profile->GetPrefs();
  bool allow_deleting_history =
      prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory);
  source->AddBoolean("allowDeletingHistory", allow_deleting_history);

  source->AddBoolean("showMenuPromo",
      !prefs->GetBoolean(prefs::kMdHistoryMenuPromoShown));

  bool group_by_domain = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kHistoryEnableGroupByDomain) || profile->IsSupervised();
  source->AddBoolean("groupByDomain", group_by_domain);

  source->AddBoolean("isGuestSession", profile->IsGuestSession());

  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  bool is_authenticated = signin_manager != nullptr &&
      signin_manager->IsAuthenticated();
  source->AddBoolean("isUserSignedIn", is_authenticated);

  source->AddResourcePath("constants.html", IDR_MD_HISTORY_CONSTANTS_HTML);
  source->AddResourcePath("constants.js", IDR_MD_HISTORY_CONSTANTS_JS);
  source->AddResourcePath("images/100/sign_in_promo.png",
                          IDR_MD_HISTORY_IMAGES_100_SIGN_IN_PROMO_PNG);
  source->AddResourcePath("images/200/sign_in_promo.png",
                          IDR_MD_HISTORY_IMAGES_200_SIGN_IN_PROMO_PNG);
  source->AddResourcePath("history.js", IDR_MD_HISTORY_HISTORY_JS);

#if BUILDFLAG(USE_VULCANIZE)
  source->AddResourcePath("app.html",
                          IDR_MD_HISTORY_APP_VULCANIZED_HTML);
  source->AddResourcePath("app.crisper.js",
                          IDR_MD_HISTORY_APP_CRISPER_JS);
  source->AddResourcePath("lazy_load.html",
                          IDR_MD_HISTORY_LAZY_LOAD_VULCANIZED_HTML);
  source->AddResourcePath("lazy_load.crisper.js",
                          IDR_MD_HISTORY_LAZY_LOAD_CRISPER_JS);
#else
  source->AddResourcePath("app.html", IDR_MD_HISTORY_APP_HTML);
  source->AddResourcePath("app.js", IDR_MD_HISTORY_APP_JS);
  source->AddResourcePath("browser_service.html",
                          IDR_MD_HISTORY_BROWSER_SERVICE_HTML);
  source->AddResourcePath("browser_service.js",
                          IDR_MD_HISTORY_BROWSER_SERVICE_JS);
  source->AddResourcePath("grouped_list.html",
                          IDR_MD_HISTORY_GROUPED_LIST_HTML);
  source->AddResourcePath("grouped_list.js", IDR_MD_HISTORY_GROUPED_LIST_JS);
  source->AddResourcePath("history_item.html",
                          IDR_MD_HISTORY_HISTORY_ITEM_HTML);
  source->AddResourcePath("history_item.js", IDR_MD_HISTORY_HISTORY_ITEM_JS);
  source->AddResourcePath("history_list.html",
                          IDR_MD_HISTORY_HISTORY_LIST_HTML);
  source->AddResourcePath("history_list.js", IDR_MD_HISTORY_HISTORY_LIST_JS);
  source->AddResourcePath("history_list_behavior.html",
                          IDR_MD_HISTORY_HISTORY_LIST_BEHAVIOR_HTML);
  source->AddResourcePath("history_list_behavior.js",
                          IDR_MD_HISTORY_HISTORY_LIST_BEHAVIOR_JS);
  source->AddResourcePath("history_toolbar.html",
                          IDR_MD_HISTORY_HISTORY_TOOLBAR_HTML);
  source->AddResourcePath("history_toolbar.js",
                          IDR_MD_HISTORY_HISTORY_TOOLBAR_JS);
  source->AddResourcePath("icons.html", IDR_MD_HISTORY_ICONS_HTML);
  source->AddResourcePath("lazy_load.html", IDR_MD_HISTORY_LAZY_LOAD_HTML);
  source->AddResourcePath("list_container.html",
                          IDR_MD_HISTORY_LIST_CONTAINER_HTML);
  source->AddResourcePath("list_container.js",
                          IDR_MD_HISTORY_LIST_CONTAINER_JS);
  source->AddResourcePath("router.html",
                          IDR_MD_HISTORY_ROUTER_HTML);
  source->AddResourcePath("router.js",
                          IDR_MD_HISTORY_ROUTER_JS);
  source->AddResourcePath("searched_label.html",
                          IDR_MD_HISTORY_SEARCHED_LABEL_HTML);
  source->AddResourcePath("searched_label.js",
                          IDR_MD_HISTORY_SEARCHED_LABEL_JS);
  source->AddResourcePath("shared_style.html",
                          IDR_MD_HISTORY_SHARED_STYLE_HTML);
  source->AddResourcePath("shared_vars.html",
                          IDR_MD_HISTORY_SHARED_VARS_HTML);
  source->AddResourcePath("side_bar.html", IDR_MD_HISTORY_SIDE_BAR_HTML);
  source->AddResourcePath("side_bar.js", IDR_MD_HISTORY_SIDE_BAR_JS);
  source->AddResourcePath("synced_device_card.html",
                          IDR_MD_HISTORY_SYNCED_DEVICE_CARD_HTML);
  source->AddResourcePath("synced_device_card.js",
                          IDR_MD_HISTORY_SYNCED_DEVICE_CARD_JS);
  source->AddResourcePath("synced_device_manager.html",
                          IDR_MD_HISTORY_SYNCED_DEVICE_MANAGER_HTML);
  source->AddResourcePath("synced_device_manager.js",
                          IDR_MD_HISTORY_SYNCED_DEVICE_MANAGER_JS);
#endif  // BUILDFLAG(USE_VULCANIZE)

  source->SetDefaultResource(IDR_MD_HISTORY_HISTORY_HTML);
  source->SetJsonPath("strings.js");

  return source;
}

}  // namespace

bool MdHistoryUI::use_test_title_ = false;

MdHistoryUI::MdHistoryUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new BrowsingHistoryHandler());
  web_ui->AddMessageHandler(new MetricsHandler());

  if (search::IsInstantExtendedAPIEnabled()) {
    web_ui->AddMessageHandler(new browser_sync::ForeignSessionHandler());
    web_ui->AddMessageHandler(new HistoryLoginHandler(
        base::Bind(&MdHistoryUI::CreateDataSource, base::Unretained(this))));
  }

  CreateDataSource();

  web_ui->RegisterMessageCallback("menuPromoShown",
      base::Bind(&MdHistoryUI::HandleMenuPromoShown, base::Unretained(this)));
}

MdHistoryUI::~MdHistoryUI() {}

// static
bool MdHistoryUI::IsEnabled(Profile* profile) {
  return base::FeatureList::IsEnabled(features::kMaterialDesignHistory) &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kHistoryEnableGroupByDomain) &&
         !profile->IsSupervised();
}

// static
void MdHistoryUI::SetEnabledForTesting(bool enabled) {
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);

  const std::string& name = features::kMaterialDesignHistory.name;
  feature_list->InitializeFromCommandLine(enabled ? name : "",
                                          enabled ? "" : name);
  base::FeatureList::ClearInstanceForTesting();
  base::FeatureList::SetInstance(std::move(feature_list));
}

// static
base::RefCountedMemory* MdHistoryUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_HISTORY_FAVICON, scale_factor);
}

void MdHistoryUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kMdHistoryMenuPromoShown, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

// TODO(lshang): Change to not re-create data source every time after we use
// unique_ptr instead of raw pointers for data source.
void MdHistoryUI::CreateDataSource() {
  Profile* profile = Profile::FromWebUI(web_ui());
  content::WebUIDataSource* data_source =
      CreateMdHistoryUIHTMLSource(profile, use_test_title_);
  content::WebUIDataSource::Add(profile, data_source);
}

void MdHistoryUI::HandleMenuPromoShown(const base::ListValue* args) {
  Profile::FromWebUI(web_ui())->GetPrefs()->SetBoolean(
      prefs::kMdHistoryMenuPromoShown, true);
  CreateDataSource();
}
