// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/search_engine_manager_handler.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/search_engines/template_url_table_model.h"
#include "chrome/common/url_constants.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

enum EngineInfoIndexes {
  ENGINE_NAME,
  ENGINE_KEYWORD,
  ENGINE_URL,
};

};  // namespace

namespace options {

SearchEngineManagerHandler::SearchEngineManagerHandler() {
}

SearchEngineManagerHandler::~SearchEngineManagerHandler() {
  if (list_controller_.get() && list_controller_->table_model())
    list_controller_->table_model()->SetObserver(NULL);
}

void SearchEngineManagerHandler::InitializeHandler() {
  list_controller_.reset(
      new KeywordEditorController(Profile::FromWebUI(web_ui())));
  DCHECK(list_controller_.get());
  list_controller_->table_model()->SetObserver(this);
}

void SearchEngineManagerHandler::InitializePage() {
  OnModelChanged();
}

void SearchEngineManagerHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "searchEngineManagerPage",
                IDS_SEARCH_ENGINES_EDITOR_WINDOW_TITLE);
  localized_strings->SetString("defaultSearchEngineListTitle",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_MAIN_SEPARATOR));
  localized_strings->SetString("otherSearchEngineListTitle",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_OTHER_SEPARATOR));
  localized_strings->SetString("extensionKeywordsListTitle",
      l10n_util::GetStringUTF16(
          IDS_SEARCH_ENGINES_EDITOR_EXTENSIONS_SEPARATOR));
  localized_strings->SetString("makeDefaultSearchEngineButton",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_MAKE_DEFAULT_BUTTON));
  localized_strings->SetString("searchEngineTableNamePlaceholder",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINE_ADD_NEW_NAME_PLACEHOLDER));
  localized_strings->SetString("searchEngineTableKeywordPlaceholder",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINE_ADD_NEW_KEYWORD_PLACEHOLDER));
  localized_strings->SetString("searchEngineTableURLPlaceholder",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINE_ADD_NEW_URL_PLACEHOLDER));
  localized_strings->SetString("editSearchEngineInvalidTitleToolTip",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_INVALID_TITLE_TT));
  localized_strings->SetString("editSearchEngineInvalidKeywordToolTip",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_INVALID_KEYWORD_TT));
  localized_strings->SetString("editSearchEngineInvalidURLToolTip",
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_INVALID_URL_TT));
}

void SearchEngineManagerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "managerSetDefaultSearchEngine",
      base::Bind(&SearchEngineManagerHandler::SetDefaultSearchEngine,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeSearchEngine",
      base::Bind(&SearchEngineManagerHandler::RemoveSearchEngine,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "editSearchEngine",
      base::Bind(&SearchEngineManagerHandler::EditSearchEngine,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "checkSearchEngineInfoValidity",
      base::Bind(&SearchEngineManagerHandler::CheckSearchEngineInfoValidity,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "searchEngineEditCancelled",
      base::Bind(&SearchEngineManagerHandler::EditCancelled,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "searchEngineEditCompleted",
      base::Bind(&SearchEngineManagerHandler::EditCompleted,
                 base::Unretained(this)));
}

void SearchEngineManagerHandler::OnModelChanged() {
  DCHECK(list_controller_.get());
  if (!list_controller_->loaded())
    return;

  // Find the default engine.
  const TemplateURL* default_engine =
      list_controller_->GetDefaultSearchProvider();
  int default_index = list_controller_->table_model()->IndexOfTemplateURL(
      default_engine);

  // Build the first list (default search engine options).
  base::ListValue defaults_list;
  int last_default_engine_index =
      list_controller_->table_model()->last_search_engine_index();
  for (int i = 0; i < last_default_engine_index; ++i) {
    // Third argument is false, as the engine is not from an extension.
    defaults_list.Append(CreateDictionaryForEngine(
        i, i == default_index, false));
  }

  // Build the second list (other search templates).
  base::ListValue others_list;
  int last_other_engine_index =
      list_controller_->table_model()->last_other_engine_index();
  if (last_default_engine_index < 0)
    last_default_engine_index = 0;
  for (int i = last_default_engine_index; i < last_other_engine_index; ++i) {
    others_list.Append(CreateDictionaryForEngine(i, i == default_index, false));
  }

  // Build the extension keywords list.
  base::ListValue keyword_list;
  if (last_other_engine_index < 0)
    last_other_engine_index = 0;
  int engine_count = list_controller_->table_model()->RowCount();
  for (int i = last_other_engine_index; i < engine_count; ++i) {
    keyword_list.Append(CreateDictionaryForEngine(i, i == default_index, true));
  }

  web_ui()->CallJavascriptFunction("SearchEngineManager.updateSearchEngineList",
                                   defaults_list, others_list, keyword_list);
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

base::DictionaryValue* SearchEngineManagerHandler::CreateDictionaryForEngine(
    int index, bool is_default, bool is_extension) {
  TemplateURLTableModel* table_model = list_controller_->table_model();
  const TemplateURL* template_url = list_controller_->GetTemplateURL(index);

  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("name",  template_url->short_name());
  dict->SetString("displayName", table_model->GetText(
    index, IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN));
  dict->SetString("keyword", table_model->GetText(
    index, IDS_SEARCH_ENGINES_EDITOR_KEYWORD_COLUMN));
  dict->SetString("url", template_url->url_ref().DisplayURL(
      UIThreadSearchTermsData(Profile::FromWebUI(web_ui()))));
  dict->SetBoolean("urlLocked", template_url->prepopulate_id() > 0);
  GURL icon_url = template_url->favicon_url();
  if (icon_url.is_valid())
    dict->SetString("iconURL", icon_url.spec());
  dict->SetString("modelIndex", base::IntToString(index));

  dict->SetBoolean("canBeRemoved",
      list_controller_->CanRemove(template_url) && !is_extension);
  dict->SetBoolean("canBeDefault",
      list_controller_->CanMakeDefault(template_url) && !is_extension);
  dict->SetBoolean("default", is_default);
  dict->SetBoolean("canBeEdited", list_controller_->CanEdit(template_url));
  dict->SetBoolean("isExtension", is_extension);
  if (template_url->GetType() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
            ->GetExtensionById(template_url->GetExtensionId(),
                               extensions::ExtensionRegistry::EVERYTHING);
    if (extension) {
      dict->Set("extension",
                extensions::util::GetExtensionInfo(extension).release());
    }
  }
  return dict;
}

void SearchEngineManagerHandler::SetDefaultSearchEngine(
    const base::ListValue* args) {
  int index;
  if (!ExtractIntegerValue(args, &index)) {
    NOTREACHED();
    return;
  }
  if (index < 0 || index >= list_controller_->table_model()->RowCount())
    return;

  list_controller_->MakeDefaultTemplateURL(index);

  content::RecordAction(
      base::UserMetricsAction("Options_SearchEngineSetDefault"));
}

void SearchEngineManagerHandler::RemoveSearchEngine(
    const base::ListValue* args) {
  int index;
  if (!ExtractIntegerValue(args, &index)) {
    NOTREACHED();
    return;
  }
  if (index < 0 || index >= list_controller_->table_model()->RowCount())
    return;

  if (list_controller_->CanRemove(list_controller_->GetTemplateURL(index))) {
    list_controller_->RemoveTemplateURL(index);
    content::RecordAction(
        base::UserMetricsAction("Options_SearchEngineRemoved"));
  }
}

void SearchEngineManagerHandler::EditSearchEngine(const base::ListValue* args) {
  int index;
  if (!ExtractIntegerValue(args, &index)) {
    NOTREACHED();
    return;
  }

  // Allow -1, which means we are adding a new engine.
  if (index < -1 || index >= list_controller_->table_model()->RowCount())
    return;

  edit_controller_.reset(new EditSearchEngineController(
      (index == -1) ? NULL : list_controller_->GetTemplateURL(index), this,
      Profile::FromWebUI(web_ui())));
}

void SearchEngineManagerHandler::OnEditedKeyword(
    TemplateURL* template_url,
    const base::string16& title,
    const base::string16& keyword,
    const std::string& url) {
  DCHECK(!url.empty());
  if (template_url)
    list_controller_->ModifyTemplateURL(template_url, title, keyword, url);
  else
    list_controller_->AddTemplateURL(title, keyword, url);
  edit_controller_.reset();
}

void SearchEngineManagerHandler::CheckSearchEngineInfoValidity(
    const base::ListValue* args)
{
  if (!edit_controller_.get())
    return;
  base::string16 name;
  base::string16 keyword;
  std::string url;
  std::string modelIndex;
  if (!args->GetString(ENGINE_NAME, &name) ||
      !args->GetString(ENGINE_KEYWORD, &keyword) ||
      !args->GetString(ENGINE_URL, &url) ||
      !args->GetString(3, &modelIndex)) {
    NOTREACHED();
    return;
  }

  base::DictionaryValue validity;
  validity.SetBoolean("name", edit_controller_->IsTitleValid(name));
  validity.SetBoolean("keyword", edit_controller_->IsKeywordValid(keyword));
  validity.SetBoolean("url", edit_controller_->IsURLValid(url));
  base::StringValue indexValue(modelIndex);
  web_ui()->CallJavascriptFunction("SearchEngineManager.validityCheckCallback",
                                   validity, indexValue);
}

void SearchEngineManagerHandler::EditCancelled(const base::ListValue* args) {
  if (!edit_controller_.get())
    return;
  edit_controller_->CleanUpCancelledAdd();
  edit_controller_.reset();
}

void SearchEngineManagerHandler::EditCompleted(const base::ListValue* args) {
  if (!edit_controller_.get())
    return;
  base::string16 name;
  base::string16 keyword;
  std::string url;
  if (!args->GetString(ENGINE_NAME, &name) ||
      !args->GetString(ENGINE_KEYWORD, &keyword) ||
      !args->GetString(ENGINE_URL, &url)) {
    NOTREACHED();
    return;
  }

  // Recheck validity.  It's possible to get here with invalid input if e.g. the
  // user calls the right JS functions directly from the web inspector.
  if (edit_controller_->IsTitleValid(name) &&
      edit_controller_->IsKeywordValid(keyword) &&
      edit_controller_->IsURLValid(url))
    edit_controller_->AcceptAddOrEdit(name, keyword, url);
}

}  // namespace options
