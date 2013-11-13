// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_ui_delegate.h"

#include "base/i18n/string_compare.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_browser_metrics.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "components/translate/common/translate_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"

TranslateUIDelegate::TranslateUIDelegate(content::WebContents* web_contents,
                                         const std::string& original_language,
                                         const std::string& target_language)
    : web_contents_(web_contents),
      original_language_index_(NO_INDEX),
      target_language_index_(NO_INDEX) {
  languages_ =
      GetSortedLanguageNames(g_browser_process->GetApplicationLocale());

  for (std::vector<LanguageNamePair>::const_iterator iter = languages_.begin();
       iter != languages_.end(); ++iter) {
    std::string language_code = iter->first;
    if (language_code == original_language)
      original_language_index_ = iter - languages_.begin();
    if (language_code == target_language)
      target_language_index_ = iter - languages_.begin();
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  prefs_.reset(new TranslatePrefs(profile->GetPrefs()));
}

TranslateUIDelegate::~TranslateUIDelegate() {
}

// static
std::vector<TranslateUIDelegate::LanguageNamePair>
    TranslateUIDelegate::GetSortedLanguageNames(const std::string& locale) {
  std::vector<std::string> language_codes;
  TranslateManager::GetSupportedLanguages(&language_codes);

  // Preparing for the alphabetical order in the locale.
  UErrorCode error = U_ZERO_ERROR;
  icu::Locale loc(locale.c_str());
  scoped_ptr<icu::Collator> collator(icu::Collator::createInstance(loc, error));
  collator->setStrength(icu::Collator::PRIMARY);

  std::vector<LanguageNamePair> language_names;
  language_names.reserve(language_codes.size());
  for (std::vector<std::string>::const_iterator iter = language_codes.begin();
       iter != language_codes.end(); ++iter) {
    std::string language_code = *iter;

    string16 language_name = l10n_util::GetDisplayNameForLocale(
        language_code, locale, true);
    // Insert the language in |language_names| in alphabetical order.
    std::vector<LanguageNamePair>::iterator iter2;
    for (iter2 = language_names.begin(); iter2 != language_names.end();
         ++iter2) {
      if (base::i18n::CompareString16WithCollator(collator.get(),
          language_name, iter2->second) == UCOL_LESS) {
        break;
      }
    }
    language_names.insert(iter2,
                          LanguageNamePair(language_code, language_name));
  }

  return language_names;
}

// static
std::string TranslateUIDelegate::GetPageHost(
    content::WebContents* web_contents) {
  content::NavigationEntry* entry =
      web_contents->GetController().GetActiveEntry();
  return entry ? entry->GetURL().HostNoBrackets() : std::string();
}

size_t TranslateUIDelegate::GetNumberOfLanguages() const {
  return languages_.size();
}

size_t TranslateUIDelegate::GetOriginalLanguageIndex() const {
  return original_language_index_;
}

void TranslateUIDelegate::UpdateOriginalLanguageIndex(size_t language_index) {
  if (original_language_index_ == language_index)
    return;

  UMA_HISTOGRAM_BOOLEAN(TranslateBrowserMetrics::GetMetricsName(
      TranslateBrowserMetrics::UMA_MODIFY_ORIGINAL_LANG), true);
  original_language_index_ = language_index;
}

size_t TranslateUIDelegate::GetTargetLanguageIndex() const {
  return target_language_index_;
}

void TranslateUIDelegate::UpdateTargetLanguageIndex(size_t language_index) {
  if (target_language_index_ == language_index)
    return;

  DCHECK_LT(language_index, GetNumberOfLanguages());
  UMA_HISTOGRAM_BOOLEAN(TranslateBrowserMetrics::GetMetricsName(
      TranslateBrowserMetrics::UMA_MODIFY_TARGET_LANG), true);
  target_language_index_ = language_index;
}


std::string TranslateUIDelegate::GetLanguageCodeAt(size_t index) const {
  DCHECK_LT(index, GetNumberOfLanguages());
  return languages_[index].first;
}

string16 TranslateUIDelegate::GetLanguageNameAt(size_t index) const {
  if (index == static_cast<size_t>(NO_INDEX))
    return string16();
  DCHECK_LT(index, GetNumberOfLanguages());
  return languages_[index].second;
}

std::string TranslateUIDelegate::GetOriginalLanguageCode() const {
  return (GetOriginalLanguageIndex() == static_cast<size_t>(NO_INDEX)) ?
      translate::kUnknownLanguageCode :
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

  UMA_HISTOGRAM_BOOLEAN(TranslateBrowserMetrics::GetMetricsName(
      TranslateBrowserMetrics::UMA_PERFORM_TRANSLATE), true);
}

void TranslateUIDelegate::RevertTranslation() {
  TranslateManager::GetInstance()->RevertTranslation(web_contents());

  UMA_HISTOGRAM_BOOLEAN(TranslateBrowserMetrics::GetMetricsName(
      TranslateBrowserMetrics::UMA_REVERT_TRANSLATION), true);
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

  UMA_HISTOGRAM_BOOLEAN(TranslateBrowserMetrics::GetMetricsName(
      TranslateBrowserMetrics::UMA_DECLINE_TRANSLATE), true);
}

bool TranslateUIDelegate::IsLanguageBlocked() {
  return prefs_->IsBlockedLanguage(GetOriginalLanguageCode());
}

void TranslateUIDelegate::SetLanguageBlocked(bool value) {
  if (value) {
    prefs_->BlockLanguage(GetOriginalLanguageCode());
    TranslateTabHelper* translate_tab_helper =
        TranslateTabHelper::FromWebContents(web_contents());
    DCHECK(translate_tab_helper);
    translate_tab_helper->language_state().SetTranslateEnabled(false);
  } else {
    prefs_->UnblockLanguage(GetOriginalLanguageCode());
  }

  UMA_HISTOGRAM_BOOLEAN(TranslateBrowserMetrics::GetMetricsName(
      TranslateBrowserMetrics::UMA_NEVER_TRANSLATE_LANG), true);
}

bool TranslateUIDelegate::IsSiteBlacklisted() {
  std::string host = GetPageHost(web_contents());
  return !host.empty() && prefs_->IsSiteBlacklisted(host);
}

void TranslateUIDelegate::SetSiteBlacklist(bool value) {
  std::string host = GetPageHost(web_contents());
  if (host.empty())
    return;

  if (value) {
    prefs_->BlacklistSite(host);
    TranslateTabHelper* translate_tab_helper =
        TranslateTabHelper::FromWebContents(web_contents());
    DCHECK(translate_tab_helper);
    translate_tab_helper->language_state().SetTranslateEnabled(false);
  } else {
    prefs_->RemoveSiteFromBlacklist(host);
  }

  UMA_HISTOGRAM_BOOLEAN(TranslateBrowserMetrics::GetMetricsName(
      TranslateBrowserMetrics::UMA_NEVER_TRANSLATE_SITE), true);
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

  UMA_HISTOGRAM_BOOLEAN(TranslateBrowserMetrics::GetMetricsName(
      TranslateBrowserMetrics::UMA_ALWAYS_TRANSLATE_LANG), true);
}
