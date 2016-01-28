// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_startup_pages_handler.h"

#include <string>
#include <vector>

#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/web_ui.h"

namespace settings {

StartupPagesHandler::StartupPagesHandler(content::WebUI* webui)
    : startup_custom_pages_table_model_(Profile::FromWebUI(webui)) {
}

StartupPagesHandler::~StartupPagesHandler() {
}

void StartupPagesHandler::RegisterMessages() {
  if (Profile::FromWebUI(web_ui())->IsOffTheRecord())
    return;

  web_ui()->RegisterMessageCallback("addStartupPage",
      base::Bind(&StartupPagesHandler::AddStartupPage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onStartupPrefsPageLoad",
      base::Bind(&StartupPagesHandler::OnStartupPrefsPageLoad,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeStartupPage",
      base::Bind(&StartupPagesHandler::RemoveStartupPage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setStartupPagesToCurrentPages",
      base::Bind(&StartupPagesHandler::SetStartupPagesToCurrentPages,
                 base::Unretained(this)));
}

void StartupPagesHandler::OnModelChanged() {
  base::ListValue startup_pages;
  int page_count = startup_custom_pages_table_model_.RowCount();
  std::vector<GURL> urls = startup_custom_pages_table_model_.GetURLs();
  for (int i = 0; i < page_count; ++i) {
    scoped_ptr<base::DictionaryValue> entry(new base::DictionaryValue());
    entry->SetString("title", startup_custom_pages_table_model_.GetText(i, 0));
    entry->SetString("url", urls[i].spec());
    entry->SetString("tooltip",
                     startup_custom_pages_table_model_.GetTooltip(i));
    entry->SetInteger("modelIndex", i);
    startup_pages.Append(entry.release());
  }

  web_ui()->CallJavascriptFunction("Settings.updateStartupPages",
                                   startup_pages);
}

void StartupPagesHandler::OnItemsChanged(int start, int length) {
  OnModelChanged();
}

void StartupPagesHandler::OnItemsAdded(int start, int length) {
  OnModelChanged();
}

void StartupPagesHandler::OnItemsRemoved(int start, int length) {
  OnModelChanged();
}

void StartupPagesHandler::AddStartupPage(const base::ListValue* args) {
  std::string url_string;
  if (!args->GetString(0, &url_string)) {
    DLOG(ERROR) << "Missing URL string parameter";
    return;
  }

  GURL url = url_formatter::FixupURL(url_string, std::string());
  if (!url.is_valid()) {
    LOG(ERROR) << "FixupURL failed on " << url_string;
    return;
  }

  int row_count = startup_custom_pages_table_model_.RowCount();
  int index;
  if (!args->GetInteger(1, &index) || index > row_count)
    index = row_count;

  startup_custom_pages_table_model_.Add(index, url);
  SaveStartupPagesPref();
}

void StartupPagesHandler::OnStartupPrefsPageLoad(const base::ListValue* args) {
  startup_custom_pages_table_model_.SetObserver(this);

  PrefService* prefService = Profile::FromWebUI(web_ui())->GetPrefs();
  SessionStartupPref pref = SessionStartupPref::GetStartupPref(
      prefService);
  startup_custom_pages_table_model_.SetURLs(pref.urls);

  if (pref.urls.empty())
    pref.type = SessionStartupPref::DEFAULT;

  pref_change_registrar_.Init(prefService);
  pref_change_registrar_.Add(
      prefs::kURLsToRestoreOnStartup,
      base::Bind(&StartupPagesHandler::UpdateStartupPages,
                 base::Unretained(this)));

  const SessionStartupPref startup_pref = SessionStartupPref::GetStartupPref(
      Profile::FromWebUI(web_ui())->GetPrefs());
  startup_custom_pages_table_model_.SetURLs(startup_pref.urls);
}

void StartupPagesHandler::RemoveStartupPage(const base::ListValue* args) {
  int selected_index;
  if (!args->GetInteger(0, &selected_index)) {
    DLOG(ERROR) << "Missing index parameter";
    return;
  }

  if (selected_index < 0 ||
      selected_index >= startup_custom_pages_table_model_.RowCount()) {
    LOG(ERROR) << "Index out of range " << selected_index;
    return;
  }

  startup_custom_pages_table_model_.Remove(selected_index);
  SaveStartupPagesPref();
}

void StartupPagesHandler::SaveStartupPagesPref() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();

  SessionStartupPref pref = SessionStartupPref::GetStartupPref(prefs);
  pref.urls = startup_custom_pages_table_model_.GetURLs();

  if (pref.urls.empty())
    pref.type = SessionStartupPref::DEFAULT;

  SessionStartupPref::SetStartupPref(prefs, pref);
}

void StartupPagesHandler::SetStartupPagesToCurrentPages(
    const base::ListValue* args) {
  startup_custom_pages_table_model_.SetToCurrentlyOpenPages();
  SaveStartupPagesPref();
}

void StartupPagesHandler::UpdateStartupPages() {
  const SessionStartupPref startup_pref = SessionStartupPref::GetStartupPref(
      Profile::FromWebUI(web_ui())->GetPrefs());
  startup_custom_pages_table_model_.SetURLs(startup_pref.urls);
  // The change will go to the JS code in the
  // StartupPagesHandler::OnModelChanged() method.
}

}  // namespace settings
