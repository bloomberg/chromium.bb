// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_infobar_delegate.h"

#include <algorithm>

#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/translate/translate_infobar_view.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/common/chrome_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// static
const size_t TranslateInfoBarDelegate::kNoIndex = static_cast<size_t>(-1);

// static
TranslateInfoBarDelegate* TranslateInfoBarDelegate::CreateDelegate(
    Type type,
    TabContents* tab_contents,
    const std::string& original_language,
    const std::string& target_language) {
  DCHECK_NE(TRANSLATION_ERROR, type);
  // The original language can only be "unknown" for the "translating"
  // infobar, which is the case when the user started a translation from the
  // context menu.
  DCHECK(type == TRANSLATING ||
      original_language != chrome::kUnknownLanguageCode);
  if ((original_language != chrome::kUnknownLanguageCode &&
          !TranslateManager::IsSupportedLanguage(original_language)) ||
      !TranslateManager::IsSupportedLanguage(target_language))
    return NULL;
  TranslateInfoBarDelegate* delegate =
      new TranslateInfoBarDelegate(type, TranslateErrors::NONE, tab_contents,
                                   original_language, target_language);
  DCHECK_NE(kNoIndex, delegate->target_language_index());
  return delegate;
}

TranslateInfoBarDelegate* TranslateInfoBarDelegate::CreateErrorDelegate(
    TranslateErrors::Type error,
    TabContents* tab_contents,
    const std::string& original_language,
    const std::string& target_language) {
  return new TranslateInfoBarDelegate(TRANSLATION_ERROR, error, tab_contents,
                                      original_language, target_language);
}

TranslateInfoBarDelegate::~TranslateInfoBarDelegate() {
}

std::string TranslateInfoBarDelegate::GetLanguageCodeAt(size_t index) const {
  DCHECK_LT(index, GetLanguageCount());
  return languages_[index].first;
}

string16 TranslateInfoBarDelegate::GetLanguageDisplayableNameAt(
    size_t index) const {
  DCHECK_LT(index, GetLanguageCount());
  return languages_[index].second;
}

std::string TranslateInfoBarDelegate::GetOriginalLanguageCode() const {
  return (original_language_index() == kNoIndex) ?
      chrome::kUnknownLanguageCode :
      GetLanguageCodeAt(original_language_index());
}

std::string TranslateInfoBarDelegate::GetTargetLanguageCode() const {
  return GetLanguageCodeAt(target_language_index());
}

void TranslateInfoBarDelegate::SetOriginalLanguage(size_t language_index) {
  DCHECK_LT(language_index, GetLanguageCount());
  original_language_index_ = language_index;
  if (infobar_view_)
    infobar_view_->OriginalLanguageChanged();
  if (type_ == AFTER_TRANSLATE)
    Translate();
}

void TranslateInfoBarDelegate::SetTargetLanguage(size_t language_index) {
  DCHECK_LT(language_index, GetLanguageCount());
  target_language_index_ = language_index;
  if (infobar_view_)
    infobar_view_->TargetLanguageChanged();
  if (type_ == AFTER_TRANSLATE)
    Translate();
}

void TranslateInfoBarDelegate::Translate() {
  const std::string& original_language_code = GetOriginalLanguageCode();
  if (!tab_contents()->profile()->IsOffTheRecord()) {
    prefs_.ResetTranslationDeniedCount(original_language_code);
    prefs_.IncrementTranslationAcceptedCount(original_language_code);
  }

  TranslateManager::GetInstance()->TranslatePage(tab_contents_,
      GetLanguageCodeAt(original_language_index()),
      GetLanguageCodeAt(target_language_index()));
}

void TranslateInfoBarDelegate::RevertTranslation() {
  TranslateManager::GetInstance()->RevertTranslation(tab_contents_);
  tab_contents_->RemoveInfoBar(this);
}

void TranslateInfoBarDelegate::ReportLanguageDetectionError() {
  TranslateManager::GetInstance()->ReportLanguageDetectionError(tab_contents_);
}

void TranslateInfoBarDelegate::TranslationDeclined() {
  const std::string& original_language_code = GetOriginalLanguageCode();
  if (!tab_contents()->profile()->IsOffTheRecord()) {
    prefs_.ResetTranslationAcceptedCount(original_language_code);
    prefs_.IncrementTranslationDeniedCount(original_language_code);
  }

  // Remember that the user declined the translation so as to prevent showing a
  // translate infobar for that page again.  (TranslateManager initiates
  // translations when getting a LANGUAGE_DETERMINED from the page, which
  // happens when a load stops. That could happen multiple times, including
  // after the user already declined the translation.)
  tab_contents_->language_state().set_translation_declined(true);
}

bool TranslateInfoBarDelegate::IsLanguageBlacklisted() {
  return prefs_.IsLanguageBlacklisted(GetOriginalLanguageCode());
}

void TranslateInfoBarDelegate::ToggleLanguageBlacklist() {
  const std::string& original_lang = GetOriginalLanguageCode();
  if (prefs_.IsLanguageBlacklisted(original_lang)) {
    prefs_.RemoveLanguageFromBlacklist(original_lang);
  } else {
    prefs_.BlacklistLanguage(original_lang);
    tab_contents_->RemoveInfoBar(this);
  }
}

bool TranslateInfoBarDelegate::IsSiteBlacklisted() {
  std::string host = GetPageHost();
  return !host.empty() && prefs_.IsSiteBlacklisted(host);
}

void TranslateInfoBarDelegate::ToggleSiteBlacklist() {
  std::string host = GetPageHost();
  if (host.empty())
    return;

  if (prefs_.IsSiteBlacklisted(host)) {
    prefs_.RemoveSiteFromBlacklist(host);
  } else {
    prefs_.BlacklistSite(host);
    tab_contents_->RemoveInfoBar(this);
  }
}

bool TranslateInfoBarDelegate::ShouldAlwaysTranslate() {
  return prefs_.IsLanguagePairWhitelisted(GetOriginalLanguageCode(),
                                          GetTargetLanguageCode());
}

void TranslateInfoBarDelegate::ToggleAlwaysTranslate() {
  const std::string& original_lang = GetOriginalLanguageCode();
  const std::string& target_lang = GetTargetLanguageCode();
  if (prefs_.IsLanguagePairWhitelisted(original_lang, target_lang))
    prefs_.RemoveLanguagePairFromWhitelist(original_lang, target_lang);
  else
    prefs_.WhitelistLanguagePair(original_lang, target_lang);
}

void TranslateInfoBarDelegate::AlwaysTranslatePageLanguage() {
  const std::string& original_lang = GetOriginalLanguageCode();
  const std::string& target_lang = GetTargetLanguageCode();
  DCHECK(!prefs_.IsLanguagePairWhitelisted(original_lang, target_lang));
  prefs_.WhitelistLanguagePair(original_lang, target_lang);
  Translate();
}

void TranslateInfoBarDelegate::NeverTranslatePageLanguage() {
  std::string original_lang = GetOriginalLanguageCode();
  DCHECK(!prefs_.IsLanguageBlacklisted(original_lang));
  prefs_.BlacklistLanguage(original_lang);
  tab_contents_->RemoveInfoBar(this);
}

string16 TranslateInfoBarDelegate::GetMessageInfoBarText() {
  if (type_ == TRANSLATING) {
    return l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_TRANSLATING_TO,
        GetLanguageDisplayableNameAt(target_language_index_));
  }

  DCHECK_EQ(TRANSLATION_ERROR, type_);
  switch (error_) {
    case TranslateErrors::NETWORK:
      return l10n_util::GetStringUTF16(
          IDS_TRANSLATE_INFOBAR_ERROR_CANT_CONNECT);
    case TranslateErrors::INITIALIZATION_ERROR:
    case TranslateErrors::TRANSLATION_ERROR:
      return l10n_util::GetStringUTF16(
          IDS_TRANSLATE_INFOBAR_ERROR_CANT_TRANSLATE);
    case TranslateErrors::UNKNOWN_LANGUAGE:
      return l10n_util::GetStringUTF16(
          IDS_TRANSLATE_INFOBAR_UNKNOWN_PAGE_LANGUAGE);
    case TranslateErrors::UNSUPPORTED_LANGUAGE:
      return l10n_util::GetStringFUTF16(
          IDS_TRANSLATE_INFOBAR_UNSUPPORTED_PAGE_LANGUAGE,
          GetLanguageDisplayableNameAt(target_language_index_));
    case TranslateErrors::IDENTICAL_LANGUAGES:
      return l10n_util::GetStringFUTF16(
          IDS_TRANSLATE_INFOBAR_ERROR_SAME_LANGUAGE,
          GetLanguageDisplayableNameAt(target_language_index_));
    default:
      NOTREACHED();
      return string16();
  }
}

string16 TranslateInfoBarDelegate::GetMessageInfoBarButtonText() {
  if (type_ != TRANSLATION_ERROR) {
    DCHECK_EQ(TRANSLATING, type_);
  } else if ((error_ != TranslateErrors::IDENTICAL_LANGUAGES) &&
             (error_ != TranslateErrors::UNKNOWN_LANGUAGE)) {
    return l10n_util::GetStringUTF16(
        (error_ == TranslateErrors::UNSUPPORTED_LANGUAGE) ?
        IDS_TRANSLATE_INFOBAR_REVERT : IDS_TRANSLATE_INFOBAR_RETRY);
  }
  return string16();
}

void TranslateInfoBarDelegate::MessageInfoBarButtonPressed() {
  DCHECK_EQ(TRANSLATION_ERROR, type_);
  if (error_ == TranslateErrors::UNSUPPORTED_LANGUAGE) {
    RevertTranslation();
    return;
  }
  // This is the "Try again..." case.
  TranslateManager::GetInstance()->TranslatePage(tab_contents_,
      GetOriginalLanguageCode(), GetTargetLanguageCode());
}

bool TranslateInfoBarDelegate::ShouldShowMessageInfoBarButton() {
  return !GetMessageInfoBarButtonText().empty();
}

bool TranslateInfoBarDelegate::ShouldShowNeverTranslateButton() {
  DCHECK_EQ(BEFORE_TRANSLATE, type_);
  return !tab_contents()->profile()->IsOffTheRecord() &&
      (prefs_.GetTranslationDeniedCount(GetOriginalLanguageCode()) >= 3);
}

bool TranslateInfoBarDelegate::ShouldShowAlwaysTranslateButton() {
  DCHECK_EQ(BEFORE_TRANSLATE, type_);
  return !tab_contents()->profile()->IsOffTheRecord() &&
      (prefs_.GetTranslationAcceptedCount(GetOriginalLanguageCode()) >= 3);
}

void TranslateInfoBarDelegate::UpdateBackgroundAnimation(
    TranslateInfoBarDelegate* previous_infobar) {
  if (!previous_infobar || previous_infobar->IsError() == IsError())
    background_animation_ = NONE;
  else
    background_animation_ = IsError() ? NORMAL_TO_ERROR : ERROR_TO_NORMAL;
}

// static
string16 TranslateInfoBarDelegate::GetLanguageDisplayableName(
    const std::string& language_code) {
  return l10n_util::GetDisplayNameForLocale(
      language_code, g_browser_process->GetApplicationLocale(), true);
}

// static
void TranslateInfoBarDelegate::GetAfterTranslateStrings(
    std::vector<string16>* strings, bool* swap_languages) {
  DCHECK(strings);
  DCHECK(swap_languages);

  std::vector<size_t> offsets;
  string16 text =
      l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_AFTER_MESSAGE,
                                 string16(), string16(), &offsets);
  DCHECK_EQ(2U, offsets.size());

  *swap_languages = (offsets[0] > offsets[1]);
  if (*swap_languages)
    std::swap(offsets[0], offsets[1]);

  strings->push_back(text.substr(0, offsets[0]));
  strings->push_back(text.substr(offsets[0], offsets[1] - offsets[0]));
  strings->push_back(text.substr(offsets[1]));
}

TranslateInfoBarDelegate::TranslateInfoBarDelegate(
    Type type,
    TranslateErrors::Type error,
    TabContents* tab_contents,
    const std::string& original_language,
    const std::string& target_language)
    : InfoBarDelegate(tab_contents),
      type_(type),
      background_animation_(NONE),
      tab_contents_(tab_contents),
      original_language_index_(kNoIndex),
      initial_original_language_index_(kNoIndex),
      target_language_index_(kNoIndex),
      error_(error),
      infobar_view_(NULL),
      prefs_(tab_contents_->profile()->GetPrefs()) {
  DCHECK_NE((type_ == TRANSLATION_ERROR), (error == TranslateErrors::NONE));

  std::vector<std::string> language_codes;
  TranslateManager::GetSupportedLanguages(&language_codes);

  languages_.reserve(language_codes.size());
  for (std::vector<std::string>::const_iterator iter = language_codes.begin();
       iter != language_codes.end(); ++iter) {
    std::string language_code = *iter;

    string16 language_name = GetLanguageDisplayableName(language_code);
    // Insert the language in languages_ in alphabetical order.
    std::vector<LanguageNamePair>::iterator iter2;
    for (iter2 = languages_.begin(); iter2 != languages_.end(); ++iter2) {
      if (language_name.compare(iter2->second) < 0)
        break;
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
}

void TranslateInfoBarDelegate::InfoBarDismissed() {
  if (type_ != BEFORE_TRANSLATE)
    return;

  // The user closed the infobar without clicking the translate button.
  TranslationDeclined();
  UMA_HISTOGRAM_COUNTS("Translate.DeclineTranslateCloseInfobar", 1);
}

void TranslateInfoBarDelegate::InfoBarClosed() {
  delete this;
}

SkBitmap* TranslateInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_TRANSLATE);
}

InfoBarDelegate::Type TranslateInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

TranslateInfoBarDelegate*
    TranslateInfoBarDelegate::AsTranslateInfoBarDelegate() {
  return this;
}

std::string TranslateInfoBarDelegate::GetPageHost() {
  NavigationEntry* entry = tab_contents_->controller().GetActiveEntry();
  return entry ? entry->url().HostNoBrackets() : std::string();
}
