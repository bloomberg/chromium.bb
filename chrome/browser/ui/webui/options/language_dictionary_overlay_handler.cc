// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/language_dictionary_overlay_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace options {

namespace {
bool StringCompare(const std::string& a, const std::string& b) {
  return a.compare(b) < 0;
}
}  // namespace

LanguageDictionaryOverlayHandler::LanguageDictionaryOverlayHandler()
    : overlay_initialized_(false),
      dictionary_(NULL) {
}

LanguageDictionaryOverlayHandler::~LanguageDictionaryOverlayHandler() {
}

void LanguageDictionaryOverlayHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  RegisterTitle(localized_strings,
                "languageDictionaryOverlayPage",
                IDS_LANGUAGE_DICTIONARY_OVERLAY_TITLE);
  localized_strings->SetString(
      "languageDictionaryOverlayTitle",
      l10n_util::GetStringUTF16(IDS_LANGUAGE_DICTIONARY_OVERLAY_TITLE));
  localized_strings->SetString(
      "languageDictionaryOverlayAddWordLabel",
      l10n_util::GetStringUTF16(
          IDS_LANGUAGE_DICTIONARY_OVERLAY_ADD_WORD_LABEL));
}

void LanguageDictionaryOverlayHandler::InitializeHandler() {
  dictionary_ = SpellcheckServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui()))->GetCustomDictionary();
  dictionary_->AddObserver(this);
}

void LanguageDictionaryOverlayHandler::InitializePage() {
}

void LanguageDictionaryOverlayHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "addDictionaryWord",
      base::Bind(&LanguageDictionaryOverlayHandler::AddWord,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeDictionaryWord",
      base::Bind(&LanguageDictionaryOverlayHandler::RemoveWord,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "refreshDictionaryWords",
      base::Bind(&LanguageDictionaryOverlayHandler::RefreshWords,
                 base::Unretained(this)));
}

void LanguageDictionaryOverlayHandler::Uninitialize() {
  overlay_initialized_ = false;
  if (dictionary_)
    dictionary_->RemoveObserver(this);
}

void LanguageDictionaryOverlayHandler::OnCustomDictionaryLoaded() {
  ResetDataModel();
  UpdateWordList();
}

void LanguageDictionaryOverlayHandler::OnCustomDictionaryWordAdded(
    const std::string& word) {
}

void LanguageDictionaryOverlayHandler::OnCustomDictionaryWordRemoved(
    const std::string& word) {
}

void LanguageDictionaryOverlayHandler::ResetDataModel() {
  // TODO(rouslan): Paginate dictionary words.
  data_model_ = dictionary_->GetWords();
  sort(data_model_.begin(), data_model_.end(), StringCompare);
}

void LanguageDictionaryOverlayHandler::UpdateWordList() {
  if (!overlay_initialized_)
    return;
  ListValue list_value;
  list_value.AppendStrings(data_model_);
  web_ui()->CallJavascriptFunction("EditDictionaryOverlay.setWordList",
                                   list_value);
}

void LanguageDictionaryOverlayHandler::RefreshWords(const ListValue* args) {
  overlay_initialized_ = true;
  ResetDataModel();
  UpdateWordList();
}

void LanguageDictionaryOverlayHandler::AddWord(const ListValue* args) {
  std::string new_word;
  if (!args->GetString(0, &new_word) || new_word.empty()) {
    NOTREACHED();
    return;
  }
  dictionary_->AddWord(new_word);
  data_model_.push_back(new_word);
  UpdateWordList();
}

void LanguageDictionaryOverlayHandler::RemoveWord(const ListValue* args) {
  std::string index_string;
  if (!args->GetString(0, &index_string) || index_string.empty()) {
    NOTREACHED();
    return;
  }
  std::stringstream ss(index_string);
  int i = -1;
  if ((ss >> i).fail() || i < 0 || i >= (int) data_model_.size()) {
    NOTREACHED();
    return;
  }
  dictionary_->RemoveWord(data_model_.at(i));
  data_model_.erase(data_model_.begin() + i);
  UpdateWordList();
}

}  // namespace options
