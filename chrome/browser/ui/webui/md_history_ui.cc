// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_history_ui.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/browsing_history_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/components_scaled_resources.h"
#include "grit/components_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/foreign_session_handler.h"
#include "chrome/browser/ui/webui/history_login_handler.h"
#endif

namespace {

content::WebUIDataSource* CreateMdHistoryUIHTMLSource(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIHistoryHost);

  // Localized strings (alphabetical order).
  source->AddLocalizedString("clearBrowsingData",
                             IDS_CLEAR_BROWSING_DATA_TITLE);
  source->AddLocalizedString("clearSearch", IDS_MD_HISTORY_CLEAR_SEARCH);
  source->AddLocalizedString("cancel", IDS_CANCEL);
  source->AddLocalizedString("delete", IDS_MD_HISTORY_DELETE);
  source->AddLocalizedString("itemsSelected", IDS_MD_HISTORY_ITEMS_SELECTED);
  source->AddLocalizedString("moreFromSite", IDS_HISTORY_MORE_FROM_SITE);
  source->AddLocalizedString("removeFromHistory", IDS_HISTORY_REMOVE_PAGE);
  source->AddLocalizedString("search", IDS_MD_HISTORY_SEARCH);
  source->AddLocalizedString("title", IDS_HISTORY_TITLE);

  bool allow_deleting_history =
      prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory);
  source->AddBoolean("allowDeletingHistory", allow_deleting_history);

  source->AddResourcePath("history_card.html",
                          IDR_MD_HISTORY_HISTORY_CARD_HTML);
  source->AddResourcePath("history_card.js", IDR_MD_HISTORY_HISTORY_CARD_JS);
  source->AddResourcePath("history_card_manager.html",
                          IDR_MD_HISTORY_HISTORY_CARD_MANAGER_HTML);
  source->AddResourcePath("history_card_manager.js",
                          IDR_MD_HISTORY_HISTORY_CARD_MANAGER_JS);
  source->AddResourcePath("history_item.html",
                          IDR_MD_HISTORY_HISTORY_ITEM_HTML);
  source->AddResourcePath("history_item.js", IDR_MD_HISTORY_HISTORY_ITEM_JS);
  source->AddResourcePath("history_toolbar.html",
                          IDR_MD_HISTORY_HISTORY_TOOLBAR_HTML);
  source->AddResourcePath("history_toolbar.js",
                          IDR_MD_HISTORY_HISTORY_TOOLBAR_JS);
  source->AddResourcePath("history.js", IDR_MD_HISTORY_HISTORY_JS);

  source->SetDefaultResource(IDR_MD_HISTORY_HISTORY_HTML);
  source->SetJsonPath("strings.js");

  return source;
}

}  // namespace

MdHistoryUI::MdHistoryUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new BrowsingHistoryHandler());
  web_ui->AddMessageHandler(new MetricsHandler());

  // On mobile we deal with foreign sessions differently.
#if !defined(OS_ANDROID)
  if (search::IsInstantExtendedAPIEnabled()) {
    web_ui->AddMessageHandler(new browser_sync::ForeignSessionHandler());
    web_ui->AddMessageHandler(new HistoryLoginHandler());
  }
#endif

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateMdHistoryUIHTMLSource(profile));
}

MdHistoryUI::~MdHistoryUI() {}

base::RefCountedMemory* MdHistoryUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_HISTORY_FAVICON, scale_factor);
}
