// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/search_engine_manager_handler.h"

#include "app/l10n_util.h"
#include "base/callback.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/keyword_editor_controller.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_table_model.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace {

enum EngineInfoIndexes {
  ENGINE_NAME,
  ENGINE_KEYWORD,
  ENGINE_URL,
};

};  // namespace

SearchEngineManagerHandler::SearchEngineManagerHandler() {
}

SearchEngineManagerHandler::~SearchEngineManagerHandler() {
  if (list_controller_.get() && list_controller_->table_model())
    list_controller_->table_model()->SetObserver(NULL);
}

void SearchEngineManagerHandler::Initialize() {
  list_controller_.reset(new KeywordEditorController(dom_ui_->GetProfile()));
  if (list_controller_.get()) {
    list_controller_->table_model()->SetObserver(this);
    OnModelChanged();
  }
}

void SearchEngineManagerHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("searchEngineManagerPage",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_WINDOW_TITLE));
  localized_strings->SetString("searchEngineTableNameHeader",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN));
  localized_strings->SetString("searchEngineTableKeywordHeader",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_KEYWORD_COLUMN));
  localized_strings->SetString("addSearchEngineButton",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_NEW_BUTTON));
  localized_strings->SetString("editSearchEngineButton",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_EDIT_BUTTON));
  localized_strings->SetString("makeDefaultSearchEngineButton",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_MAKE_DEFAULT_BUTTON));
  // Overlay strings.
  localized_strings->SetString("editSearchEngineTitle",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_EDIT_WINDOW_TITLE));
  localized_strings->SetString("editSearchEngineNameLabel",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_LABEL));
  localized_strings->SetString("editSearchEngineKeywordLabel",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_KEYWORD_LABEL));
  localized_strings->SetString("editSearchEngineURLLabel",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_URL_LABEL));
  localized_strings->SetString("editSearchEngineInvalidTitleToolTip",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_INVALID_TITLE_TT));
  localized_strings->SetString("editSearchEngineInvalidKeywordToolTip",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_INVALID_KEYWORD_TT));
  localized_strings->SetString("editSearchEngineInvalidURLToolTip",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_INVALID_URL_TT));
  localized_strings->SetString("editSearchEngineURLExplanation",
      l10n_util::GetStringUTF16(
          IDS_SEARCH_ENGINES_EDITOR_URL_DESCRIPTION_LABEL));
  localized_strings->SetString("editSearchEngineOkayButton",
      l10n_util::GetStringUTF16(IDS_OK));
  localized_strings->SetString("editSearchEngineCancelButton",
      l10n_util::GetStringUTF16(IDS_CANCEL));
}

void SearchEngineManagerHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback(
      "managerSetDefaultSearchEngine",
      NewCallback(this, &SearchEngineManagerHandler::SetDefaultSearchEngine));
  dom_ui_->RegisterMessageCallback(
      "removeSearchEngine",
      NewCallback(this, &SearchEngineManagerHandler::RemoveSearchEngine));
  dom_ui_->RegisterMessageCallback(
      "editSearchEngine",
      NewCallback(this, &SearchEngineManagerHandler::EditSearchEngine));
  dom_ui_->RegisterMessageCallback(
      "checkSearchEngineInfoValidity",
      NewCallback(this,
                  &SearchEngineManagerHandler::CheckSearchEngineInfoValidity));
  dom_ui_->RegisterMessageCallback(
      "searchEngineEditCancelled",
      NewCallback(this, &SearchEngineManagerHandler::EditCancelled));
  dom_ui_->RegisterMessageCallback(
      "searchEngineEditCompleted",
      NewCallback(this, &SearchEngineManagerHandler::EditCompleted));
}

void SearchEngineManagerHandler::OnModelChanged() {
  if (!list_controller_->loaded())
    return;

  ListValue engine_list;

  // Find the default engine.
  const TemplateURL* default_engine =
      list_controller_->url_model()->GetDefaultSearchProvider();
  int default_index = list_controller_->table_model()->IndexOfTemplateURL(
      default_engine);

  // Add the first group (default search engine options).
  engine_list.Append(CreateDictionaryForHeading(0));
  int last_default_engine_index =
      list_controller_->table_model()->last_search_engine_index();
  for (int i = 0; i < last_default_engine_index; ++i) {
    engine_list.Append(CreateDictionaryForEngine(i, i == default_index));
  }

  // Add the second group (other search templates).
  engine_list.Append(CreateDictionaryForHeading(1));
  if (last_default_engine_index < 0)
    last_default_engine_index = 0;
  int engine_count = list_controller_->table_model()->RowCount();
  for (int i = last_default_engine_index; i < engine_count; ++i) {
    engine_list.Append(CreateDictionaryForEngine(i, i == default_index));
  }

  dom_ui_->CallJavascriptFunction(L"SearchEngineManager.updateSearchEngineList",
                                  engine_list);
}

void SearchEngineManagerHandler::OnItemsChanged(int start, int length) {
  OnModelChanged();
}

void SearchEngineManagerHandler::OnItemsAdded(int start, int length) {
  OnModelChanged();
}

void SearchEngineManagerHandler::OnItemsRemoved(int start, int length) {
  OnModelChanged();
}

DictionaryValue* SearchEngineManagerHandler::CreateDictionaryForHeading(
    int group_index) {
  TableModel::Groups groups = list_controller_->table_model()->GetGroups();

  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("heading", groups[group_index].title);
  return dict;
}

DictionaryValue* SearchEngineManagerHandler::CreateDictionaryForEngine(
    int index, bool is_default) {
  TemplateURLTableModel* table_model = list_controller_->table_model();

  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("name", table_model->GetText(
    index, IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN));
  dict->SetString("keyword", table_model->GetText(
    index, IDS_SEARCH_ENGINES_EDITOR_KEYWORD_COLUMN));
  const TemplateURL* template_url = list_controller_->GetTemplateURL(index);
  GURL icon_url = template_url->GetFavIconURL();
  if (icon_url.is_valid())
    dict->SetString("iconURL", icon_url.spec());
  dict->SetString("modelIndex", base::IntToString(index));

  if (list_controller_->CanRemove(template_url))
    dict->SetString("canBeRemoved", "1");
  if (list_controller_->CanMakeDefault(template_url))
    dict->SetString("canBeDefault", "1");
  if (is_default)
    dict->SetString("default", "1");

  return dict;
}

void SearchEngineManagerHandler::SetDefaultSearchEngine(const ListValue* args) {
  int index;
  if (!ExtractIntegerValue(args, &index)) {
    NOTREACHED();
    return;
  }
  if (index < 0 || index >= list_controller_->table_model()->RowCount())
    return;

  list_controller_->MakeDefaultTemplateURL(index);
}

void SearchEngineManagerHandler::RemoveSearchEngine(const ListValue* args) {
  int index;
  if (!ExtractIntegerValue(args, &index)) {
    NOTREACHED();
    return;
  }
  if (index < 0 || index >= list_controller_->table_model()->RowCount())
    return;

  if (list_controller_->CanRemove(list_controller_->GetTemplateURL(index)))
    list_controller_->RemoveTemplateURL(index);
}

void SearchEngineManagerHandler::EditSearchEngine(const ListValue* args) {
  int index;
  if (!ExtractIntegerValue(args, &index)) {
    NOTREACHED();
    return;
  }
  // Allow -1, which means we are adding a new engine.
  if (index < -1 || index >= list_controller_->table_model()->RowCount())
    return;

  const TemplateURL* edit_url = NULL;
  if (index != -1)
    edit_url = list_controller_->GetTemplateURL(index);
  edit_controller_.reset(
      new EditSearchEngineController(edit_url, this, dom_ui_->GetProfile()));

  if (edit_url) {
    DictionaryValue engine_details;
    engine_details.SetString("name", WideToUTF16Hack(edit_url->short_name()));
    engine_details.SetString("keyword", WideToUTF16Hack(edit_url->keyword()));
    engine_details.SetString("url",
                             WideToUTF16Hack(edit_url->url()->DisplayURL()));
    engine_details.SetBoolean("urlLocked", edit_url->prepopulate_id() > 0);
    dom_ui_->CallJavascriptFunction(L"EditSearchEngineOverlay.setEditDetails",
                                    engine_details);
  }
}

void SearchEngineManagerHandler::OnEditedKeyword(
    const TemplateURL* template_url,
    const string16& title,
    const string16& keyword,
    const std::string& url) {
  if (template_url) {
    list_controller_->ModifyTemplateURL(template_url, title, keyword, url);
  } else {
    list_controller_->AddTemplateURL(title, keyword, url);
  }
  edit_controller_.reset();
}

void SearchEngineManagerHandler::CheckSearchEngineInfoValidity(
    const ListValue* args)
{
  if (!edit_controller_.get())
    return;
  string16 name;
  string16 keyword;
  std::string url;
  if (!args->GetString(ENGINE_NAME, &name) ||
      !args->GetString(ENGINE_KEYWORD, &keyword) ||
      !args->GetString(ENGINE_URL, &url)) {
    NOTREACHED();
    return;
  }

  DictionaryValue validity;
  validity.SetBoolean("name", edit_controller_->IsTitleValid(name));
  validity.SetBoolean("keyword", edit_controller_->IsKeywordValid(keyword));
  validity.SetBoolean("url", edit_controller_->IsURLValid(url));
  dom_ui_->CallJavascriptFunction(
      L"EditSearchEngineOverlay.validityCheckCallback", validity);
}

void SearchEngineManagerHandler::EditCancelled(const ListValue* args) {
  if (!edit_controller_.get())
    return;
  edit_controller_->CleanUpCancelledAdd();
  edit_controller_.reset();
}

void SearchEngineManagerHandler::EditCompleted(const ListValue* args) {
  if (!edit_controller_.get())
    return;
  string16 name;
  string16 keyword;
  std::string url;
  if (!args->GetString(ENGINE_NAME, &name) ||
      !args->GetString(ENGINE_KEYWORD, &keyword) ||
      !args->GetString(ENGINE_URL, &url)) {
    NOTREACHED();
    return;
  }
  edit_controller_->AcceptAddOrEdit(name, keyword, url);
}
