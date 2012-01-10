// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/startup_pages_handler2.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_controller_delegate.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/custom_home_pages_table_model.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "grit/generated_resources.h"

namespace options2 {

StartupPagesHandler::StartupPagesHandler()
    : autocomplete_controller_(NULL),
      startup_custom_pages_table_model_(NULL) {
}

StartupPagesHandler::~StartupPagesHandler() {

}

void StartupPagesHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "startupAddLabel", IDS_OPTIONS_STARTUP_ADD_LABEL },
    { "startupPagesDialogTitle", IDS_OPTIONS2_STARTUP_PAGES_DIALOG_TITLE },
    { "startupUseCurrent", IDS_OPTIONS_STARTUP_USE_CURRENT },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));

}

void StartupPagesHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("removeStartupPages",
      base::Bind(&StartupPagesHandler::RemoveStartupPages,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("addStartupPage",
      base::Bind(&StartupPagesHandler::AddStartupPage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("editStartupPage",
      base::Bind(&StartupPagesHandler::EditStartupPage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setStartupPagesToCurrentPages",
      base::Bind(&StartupPagesHandler::SetStartupPagesToCurrentPages,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("dragDropStartupPage",
      base::Bind(&StartupPagesHandler::DragDropStartupPage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestAutocompleteSuggestionsForStartupPages",
      base::Bind(&StartupPagesHandler::RequestAutocompleteSuggestions,
                 base::Unretained(this)));
}

void StartupPagesHandler::UpdateStartupPages() {
  Profile* profile = Profile::FromWebUI(web_ui());
  const SessionStartupPref startup_pref =
      SessionStartupPref::GetStartupPref(profile->GetPrefs());
  startup_custom_pages_table_model_->SetURLs(startup_pref.urls);
}

void StartupPagesHandler::Initialize() {
  Profile* profile = Profile::FromWebUI(web_ui());

  startup_custom_pages_table_model_.reset(
      new CustomHomePagesTableModel(profile));
  startup_custom_pages_table_model_->SetObserver(this);
  UpdateStartupPages();

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(prefs::kURLsToRestoreOnStartup, this);

  autocomplete_controller_.reset(new AutocompleteController(profile, this));
}

void StartupPagesHandler::OnModelChanged() {
  ListValue startup_pages;
  int page_count = startup_custom_pages_table_model_->RowCount();
  std::vector<GURL> urls = startup_custom_pages_table_model_->GetURLs();
  for (int i = 0; i < page_count; ++i) {
    DictionaryValue* entry = new DictionaryValue();
    entry->SetString("title", startup_custom_pages_table_model_->GetText(i, 0));
    entry->SetString("url", urls[i].spec());
    entry->SetString("tooltip",
                     startup_custom_pages_table_model_->GetTooltip(i));
    entry->SetString("modelIndex", base::IntToString(i));
    startup_pages.Append(entry);
  }

  web_ui()->CallJavascriptFunction("StartupOverlay.updateStartupPages",
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

void StartupPagesHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    std::string* pref = content::Details<std::string>(details).ptr();
    if (*pref == prefs::kURLsToRestoreOnStartup) {
      UpdateStartupPages();
    } else {
      NOTREACHED();
    }
  } else {
    NOTREACHED();
  }
}

void StartupPagesHandler::SetStartupPagesToCurrentPages(
    const ListValue* args) {
  startup_custom_pages_table_model_->SetToCurrentlyOpenPages();
  SaveStartupPagesPref();
}

void StartupPagesHandler::RemoveStartupPages(const ListValue* args) {
  for (int i = args->GetSize() - 1; i >= 0; --i) {
    std::string string_value;
    CHECK(args->GetString(i, &string_value));

    int selected_index;
    base::StringToInt(string_value, &selected_index);
    if (selected_index < 0 ||
        selected_index >= startup_custom_pages_table_model_->RowCount()) {
      NOTREACHED();
      return;
    }
    startup_custom_pages_table_model_->Remove(selected_index);
  }

  SaveStartupPagesPref();
}

void StartupPagesHandler::AddStartupPage(const ListValue* args) {
  std::string url_string;
  CHECK_EQ(args->GetSize(), 1U);
  CHECK(args->GetString(0, &url_string));

  GURL url = URLFixerUpper::FixupURL(url_string, std::string());
  if (!url.is_valid())
    return;
  int index = startup_custom_pages_table_model_->RowCount();
  startup_custom_pages_table_model_->Add(index, url);
  SaveStartupPagesPref();
}

void StartupPagesHandler::EditStartupPage(const ListValue* args) {
  std::string url_string;
  std::string index_string;
  int index;
  CHECK_EQ(args->GetSize(), 2U);
  CHECK(args->GetString(0, &index_string));
  CHECK(base::StringToInt(index_string, &index));
  CHECK(args->GetString(1, &url_string));

  if (index < 0 || index > startup_custom_pages_table_model_->RowCount()) {
    NOTREACHED();
    return;
  }

  std::vector<GURL> urls = startup_custom_pages_table_model_->GetURLs();
  urls[index] = URLFixerUpper::FixupURL(url_string, std::string());
  startup_custom_pages_table_model_->SetURLs(urls);
  SaveStartupPagesPref();
}

void StartupPagesHandler::DragDropStartupPage(const ListValue* args) {
  CHECK_EQ(args->GetSize(), 2U);

  std::string value;
  int to_index;

  CHECK(args->GetString(0, &value));
  base::StringToInt(value, &to_index);

  ListValue* selected;
  CHECK(args->GetList(1, &selected));

  std::vector<int> index_list;
  for (size_t i = 0; i < selected->GetSize(); ++i) {
    int index;
    CHECK(selected->GetString(i, &value));
    base::StringToInt(value, &index);
    index_list.push_back(index);
  }

  startup_custom_pages_table_model_->MoveURLs(to_index, index_list);
  SaveStartupPagesPref();
}

void StartupPagesHandler::SaveStartupPagesPref() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();

  SessionStartupPref pref = SessionStartupPref::GetStartupPref(prefs);
  pref.urls = startup_custom_pages_table_model_->GetURLs();

  SessionStartupPref::SetStartupPref(prefs, pref);
}

void StartupPagesHandler::RequestAutocompleteSuggestions(
    const ListValue* args) {
  string16 input;
  CHECK_EQ(args->GetSize(), 1U);
  CHECK(args->GetString(0, &input));

  autocomplete_controller_->Start(input, string16(), true, false, false,
                                  AutocompleteInput::ALL_MATCHES);
}

void StartupPagesHandler::OnResultChanged(bool default_match_changed) {
  const AutocompleteResult& result = autocomplete_controller_->result();
  ListValue suggestions;
  for (size_t i = 0; i < result.size(); ++i) {
    const AutocompleteMatch& match = result.match_at(i);
    AutocompleteMatch::Type type = match.type;
    if (type != AutocompleteMatch::HISTORY_URL &&
        type != AutocompleteMatch::HISTORY_TITLE &&
        type != AutocompleteMatch::HISTORY_BODY &&
        type != AutocompleteMatch::HISTORY_KEYWORD &&
        type != AutocompleteMatch::NAVSUGGEST)
      continue;
    DictionaryValue* entry = new DictionaryValue();
    entry->SetString("title", match.description);
    entry->SetString("displayURL", match.contents);
    entry->SetString("url", match.destination_url.spec());
    suggestions.Append(entry);
  }

  web_ui()->CallJavascriptFunction(
      "StartupOverlay.updateAutocompleteSuggestions", suggestions);
}

}  // namespace options2
