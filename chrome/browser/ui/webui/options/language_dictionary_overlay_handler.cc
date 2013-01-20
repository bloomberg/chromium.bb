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
  localized_strings->SetString(
      "languageDictionaryOverlaySearchPlaceholder",
      l10n_util::GetStringUTF16(
          IDS_LANGUAGE_DICTIONARY_OVERLAY_SEARCH_PLACEHOLDER));
  localized_strings->SetString(
      "languageDictionaryOverlayNoMatches",
      l10n_util::GetStringUTF16(IDS_LANGUAGE_DICTIONARY_OVERLAY_NO_MATCHES));
}

void LanguageDictionaryOverlayHandler::InitializeHandler() {
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
  ResetDictionaryWords();
}

void LanguageDictionaryOverlayHandler::OnCustomDictionaryWordAdded(
    const std::string& word) {
  ListValue add_words;
  ListValue remove_words;
  add_words.AppendString(word);
  web_ui()->CallJavascriptFunction("EditDictionaryOverlay.updateWords",
                                   add_words,
                                   remove_words);
}

void LanguageDictionaryOverlayHandler::OnCustomDictionaryWordRemoved(
    const std::string& word) {
  ListValue add_words;
  ListValue remove_words;
  remove_words.AppendString(word);
  web_ui()->CallJavascriptFunction("EditDictionaryOverlay.updateWords",
                                   add_words,
                                   remove_words);
}

void LanguageDictionaryOverlayHandler::ResetDictionaryWords() {
  if (!overlay_initialized_)
    return;

  if (!dictionary_) {
    dictionary_ = SpellcheckServiceFactory::GetForProfile(
        Profile::FromWebUI(web_ui()))->GetCustomDictionary();
    dictionary_->AddObserver(this);
  }

  ListValue list_value;
  list_value.AppendStrings(dictionary_->GetWords());
  web_ui()->CallJavascriptFunction("EditDictionaryOverlay.setWordList",
                                   list_value);
}

void LanguageDictionaryOverlayHandler::RefreshWords(const ListValue* args) {
  overlay_initialized_ = true;
  ResetDictionaryWords();
}

void LanguageDictionaryOverlayHandler::AddWord(const ListValue* args) {
  std::string new_word;
  if (!args->GetString(0, &new_word) || new_word.empty() || !dictionary_) {
    NOTREACHED();
    return;
  }
  dictionary_->AddWord(new_word);
}

void LanguageDictionaryOverlayHandler::RemoveWord(const ListValue* args) {
  std::string old_word;
  if (!args->GetString(0, &old_word) || old_word.empty() || !dictionary_) {
    NOTREACHED();
    return;
  }
  dictionary_->RemoveWord(old_word);
}

}  // namespace options
