// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_ui_delegate.h"

#include "base/i18n/string_compare.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kDeclineTranslate[] = "Translate.DeclineTranslate";
const char kRevertTranslation[] = "Translate.RevertTranslation";
const char kPerformTranslate[] = "Translate.Translate";
const char kNeverTranslateLang[] = "Translate.NeverTranslateLang";
const char kNeverTranslateSite[] = "Translate.NeverTranslateSite";
const char kAlwaysTranslateLang[] = "Translate.AlwaysTranslateLang";

}

// static
const size_t TranslateUIDelegate::kNoIndex = static_cast<size_t>(-1);

TranslateUIDelegate::TranslateUIDelegate(content::WebContents* web_contents,
                                         const std::string& original_language,
                                         const std::string& target_language)
    : web_contents_(web_contents),
      original_language_index_(kNoIndex),
      initial_original_language_index_(kNoIndex),
      target_language_index_(kNoIndex) {
  std::vector<std::string> language_codes;
  TranslateManager::GetSupportedLanguages(&language_codes);

  // Preparing for the alphabetical order in the locale.
  UErrorCode error = U_ZERO_ERROR;
  std::string locale = g_browser_process->GetApplicationLocale();
  icu::Locale loc(locale.c_str());
  scoped_ptr<icu::Collator> collator(icu::Collator::createInstance(loc, error));
  collator->setStrength(icu::Collator::PRIMARY);

  languages_.reserve(language_codes.size());
  for (std::vector<std::string>::const_iterator iter = language_codes.begin();
       iter != language_codes.end(); ++iter) {
    std::string language_code = *iter;

    string16 language_name = l10n_util::GetDisplayNameForLocale(
        language_code, g_browser_process->GetApplicationLocale(), true);
    // Insert the language in languages_ in alphabetical order.
    std::vector<LanguageNamePair>::iterator iter2;
    for (iter2 = languages_.begin(); iter2 != languages_.end(); ++iter2) {
      if (base::i18n::CompareString16WithCollator(collator.get(),
          language_name, iter2->second) == UCOL_LESS) {
        break;
      }
    }
    languages_.insert(iter2, LanguageNamePair(language_code, language_name));
  }
  for (std::vector<LanguageNamePair>::const_iterator iter = languages_.begin();
       iter != languages_.end(); ++iter) {
    std::string language_code = iter->first;
    if (language_code == original_language) {
      original_language_index_ = iter - languages_.begin();
      initial_original_language_index_ = original_language_index_;
    }
    if (language_code == target_language)
      target_language_index_ = iter - languages_.begin();
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  prefs_.reset(new TranslatePrefs(profile->GetPrefs()));
}

TranslateUIDelegate::~TranslateUIDelegate() {
}

size_t TranslateUIDelegate::GetNumberOfLanguages() const {
  return languages_.size();
}

size_t TranslateUIDelegate::GetOriginalLanguageIndex() const {
  return original_language_index_;
}

void TranslateUIDelegate::SetOriginalLanguageIndex(size_t language_index) {
  DCHECK_LT(language_index, GetNumberOfLanguages());
  original_language_index_ = language_index;
}

size_t TranslateUIDelegate::GetTargetLanguageIndex() const {
  return target_language_index_;
}

void TranslateUIDelegate::SetTargetLanguageIndex(size_t language_index) {
  DCHECK_LT(language_index, GetNumberOfLanguages());
  target_language_index_ = language_index;
}


std::string TranslateUIDelegate::GetLanguageCodeAt(size_t index) const {
  DCHECK_LT(index, GetNumberOfLanguages());
  return languages_[index].first;
}

string16 TranslateUIDelegate::GetLanguageNameAt(size_t index) const {
  if (index == kNoIndex)
    return string16();
  DCHECK_LT(index, GetNumberOfLanguages());
  return languages_[index].second;
}

std::string TranslateUIDelegate::GetOriginalLanguageCode() const {
  return (GetOriginalLanguageIndex() == kNoIndex) ?
      chrome::kUnknownLanguageCode :
      GetLanguageCodeAt(GetOriginalLanguageIndex());
}

std::string TranslateUIDelegate::GetTargetLanguageCode() const {
  return GetLanguageCodeAt(GetTargetLanguageIndex());
}

void TranslateUIDelegate::Translate() {
  if (!web_contents()->GetBrowserContext()->IsOffTheRecord()) {
    prefs_->ResetTranslationDeniedCount(GetOriginalLanguageCode());
    prefs_->IncrementTranslationAcceptedCount(GetOriginalLanguageCode());
  }
  TranslateManager::GetInstance()->TranslatePage(web_contents(),
                                                 GetOriginalLanguageCode(),
                                                 GetTargetLanguageCode());

  UMA_HISTOGRAM_BOOLEAN(kPerformTranslate, true);
}

void TranslateUIDelegate::RevertTranslation() {
  TranslateManager::GetInstance()->RevertTranslation(web_contents());

  UMA_HISTOGRAM_BOOLEAN(kRevertTranslation, true);
}

void TranslateUIDelegate::TranslationDeclined() {
  if (!web_contents()->GetBrowserContext()->IsOffTheRecord()) {
    prefs_->ResetTranslationAcceptedCount(GetOriginalLanguageCode());
    prefs_->IncrementTranslationDeniedCount(GetOriginalLanguageCode());
  }

  // Remember that the user declined the translation so as to prevent showing a
  // translate infobar for that page again.  (TranslateManager initiates
  // translations when getting a LANGUAGE_DETERMINED from the page, which
  // happens when a load stops. That could happen multiple times, including
  // after the user already declined the translation.)
  TranslateTabHelper::FromWebContents(web_contents())->
      language_state().set_translation_declined(true);

  UMA_HISTOGRAM_BOOLEAN(kDeclineTranslate, true);
}

bool TranslateUIDelegate::IsLanguageBlocked() {
  return prefs_->IsBlockedLanguage(GetOriginalLanguageCode());
}

void TranslateUIDelegate::SetLanguageBlocked(bool value) {
  if (value)
    prefs_->BlockLanguage(GetOriginalLanguageCode());
  else
    prefs_->UnblockLanguage(GetOriginalLanguageCode());

  UMA_HISTOGRAM_BOOLEAN(kNeverTranslateLang, true);
}

bool TranslateUIDelegate::IsSiteBlacklisted() {
  std::string host = GetPageHost();
  return !host.empty() && prefs_->IsSiteBlacklisted(host);
}

void TranslateUIDelegate::SetSiteBlacklist(bool value) {
  std::string host = GetPageHost();
  if (host.empty())
    return;

  if (value)
    prefs_->BlacklistSite(host);
  else
    prefs_->RemoveSiteFromBlacklist(host);

  UMA_HISTOGRAM_BOOLEAN(kNeverTranslateSite, true);
}

bool TranslateUIDelegate::ShouldAlwaysTranslate() {
  return prefs_->IsLanguagePairWhitelisted(GetOriginalLanguageCode(),
                                           GetTargetLanguageCode());
}

void TranslateUIDelegate::SetAlwaysTranslate(bool value) {
  const std::string& original_lang = GetOriginalLanguageCode();
  const std::string& target_lang = GetTargetLanguageCode();
  if (value)
    prefs_->WhitelistLanguagePair(original_lang, target_lang);
  else
    prefs_->RemoveLanguagePairFromWhitelist(original_lang, target_lang);

  UMA_HISTOGRAM_BOOLEAN(kAlwaysTranslateLang, true);
}

std::string TranslateUIDelegate::GetPageHost() {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetActiveEntry();
  return entry ? entry->GetURL().HostNoBrackets() : std::string();
}
