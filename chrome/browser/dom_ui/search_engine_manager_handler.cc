// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/search_engine_manager_handler.h"

#include "app/l10n_util.h"
#include "base/callback.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/keyword_editor_controller.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_table_model.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

SearchEngineManagerHandler::SearchEngineManagerHandler() {
}

SearchEngineManagerHandler::~SearchEngineManagerHandler() {
  if (controller_.get() && controller_->table_model())
    controller_->table_model()->SetObserver(NULL);
}

void SearchEngineManagerHandler::Initialize() {
  controller_.reset(new KeywordEditorController(dom_ui_->GetProfile()));
  if (controller_.get()) {
    controller_->table_model()->SetObserver(this);
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
  localized_strings->SetString("removeSearchEngineButton",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_REMOVE_BUTTON));
  localized_strings->SetString("editSearchEngineButton",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_EDIT_BUTTON));
  localized_strings->SetString("makeDefaultSearchEngineButton",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_MAKE_DEFAULT_BUTTON));
}

void SearchEngineManagerHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback(
      "managerSetDefaultSearchEngine",
      NewCallback(this, &SearchEngineManagerHandler::SetDefaultSearchEngine));
  dom_ui_->RegisterMessageCallback(
      "removeSearchEngine",
      NewCallback(this, &SearchEngineManagerHandler::RemoveSearchEngine));
}

void SearchEngineManagerHandler::OnModelChanged() {
  if (!controller_->loaded())
    return;

  ListValue engine_list;

  // Find the default engine.
  const TemplateURL* default_engine =
      controller_->url_model()->GetDefaultSearchProvider();
  int default_index = controller_->table_model()->IndexOfTemplateURL(
      default_engine);

  // Add the first group (default search engine options).
  engine_list.Append(CreateDictionaryForHeading(0));
  int last_default_engine_index =
      controller_->table_model()->last_search_engine_index();
  for (int i = 0; i < last_default_engine_index; ++i) {
    engine_list.Append(CreateDictionaryForEngine(i, i == default_index));
  }

  // Add the second group (other search templates).
  engine_list.Append(CreateDictionaryForHeading(1));
  if (last_default_engine_index < 0)
    last_default_engine_index = 0;
  int engine_count = controller_->table_model()->RowCount();
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
  TableModel::Groups groups = controller_->table_model()->GetGroups();

  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("heading", WideToUTF16Hack(groups[group_index].title));
  return dict;
}

DictionaryValue* SearchEngineManagerHandler::CreateDictionaryForEngine(
    int index, bool is_default) {
  TemplateURLTableModel* table_model = controller_->table_model();

  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("name", WideToUTF16Hack(table_model->GetText(
    index, IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN)));
  dict->SetString("keyword", WideToUTF16Hack(table_model->GetText(
    index, IDS_SEARCH_ENGINES_EDITOR_KEYWORD_COLUMN)));
  const TemplateURL* template_url = controller_->GetTemplateURL(index);
  GURL icon_url = template_url->GetFavIconURL();
  if (icon_url.is_valid())
    dict->SetString("iconURL", icon_url.spec());
  dict->SetString("modelIndex", base::IntToString(index));

  if (controller_->CanRemove(template_url))
    dict->SetString("canBeRemoved", "1");
  if (controller_->CanMakeDefault(template_url))
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
  if (index < 0 || index >= controller_->table_model()->RowCount())
    return;

  controller_->MakeDefaultTemplateURL(index);
}

void SearchEngineManagerHandler::RemoveSearchEngine(const ListValue* args) {
  int index;
  if (!ExtractIntegerValue(args, &index)) {
    NOTREACHED();
    return;
  }
  if (index < 0 || index >= controller_->table_model()->RowCount())
    return;

  if (controller_->CanRemove(controller_->GetTemplateURL(index)))
    controller_->RemoveTemplateURL(index);
}
