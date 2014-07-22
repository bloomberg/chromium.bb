// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_download_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/prefs/pref_service.h"
#include "components/translate/core/common/translate_pref_names.h"
#include "components/translate/core/common/translate_switches.h"

namespace translate {

// static
TranslateDownloadManager* TranslateDownloadManager::GetInstance() {
  return Singleton<TranslateDownloadManager>::get();
}

TranslateDownloadManager::TranslateDownloadManager()
    : language_list_(new TranslateLanguageList),
      script_(new TranslateScript) {}

TranslateDownloadManager::~TranslateDownloadManager() {}

void TranslateDownloadManager::Shutdown() {
  language_list_.reset();
  script_.reset();
  request_context_ = NULL;
}

// static
void TranslateDownloadManager::RequestLanguageList() {
  TranslateLanguageList* language_list = GetInstance()->language_list();
  if (!language_list) {
    NOTREACHED();
    return;
  }

  language_list->RequestLanguageList();
}

// static
void TranslateDownloadManager::RequestLanguageList(PrefService* prefs) {
  // We don't want to do this when translate is disabled.
  DCHECK(prefs != NULL);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          translate::switches::kDisableTranslate) ||
      !prefs->GetBoolean(prefs::kEnableTranslate)) {
    return;
  }

  RequestLanguageList();
}

// static
void TranslateDownloadManager::GetSupportedLanguages(
    std::vector<std::string>* languages) {
  TranslateLanguageList* language_list = GetInstance()->language_list();
  if (!language_list) {
    NOTREACHED();
    return;
  }

  language_list->GetSupportedLanguages(languages);
}

// static
base::Time TranslateDownloadManager::GetSupportedLanguagesLastUpdated() {
  TranslateLanguageList* language_list = GetInstance()->language_list();
  if (!language_list) {
    NOTREACHED();
    return base::Time();
  }

  return language_list->last_updated();
}

// static
std::string TranslateDownloadManager::GetLanguageCode(
    const std::string& language) {
  TranslateLanguageList* language_list = GetInstance()->language_list();
  if (!language_list) {
    NOTREACHED();
    return language;
  }

  return language_list->GetLanguageCode(language);
}

// static
bool TranslateDownloadManager::IsSupportedLanguage(
    const std::string& language) {
  TranslateLanguageList* language_list = GetInstance()->language_list();
  if (!language_list) {
    NOTREACHED();
    return false;
  }

  return language_list->IsSupportedLanguage(language);
}

// static
bool TranslateDownloadManager::IsAlphaLanguage(const std::string& language) {
  TranslateLanguageList* language_list = GetInstance()->language_list();
  if (!language_list) {
    NOTREACHED();
    return false;
  }

  return language_list->IsAlphaLanguage(language);
}

void TranslateDownloadManager::ClearTranslateScriptForTesting() {
  if (script_.get() == NULL) {
    NOTREACHED();
    return;
  }
  script_->Clear();
}

void TranslateDownloadManager::ResetForTesting() {
  language_list_.reset(new TranslateLanguageList);
  script_.reset(new TranslateScript);
}

void TranslateDownloadManager::SetTranslateScriptExpirationDelay(int delay_ms) {
  if (script_.get() == NULL) {
    NOTREACHED();
    return;
  }
  script_->set_expiration_delay(delay_ms);
}

}  // namespace translate
