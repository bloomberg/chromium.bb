// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/browser/translate/translate_infobar_delegate2.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/translate/translate_infobar_view.h"
#include "chrome/browser/translate/translate_manager2.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

// static
TranslateInfoBarDelegate2* TranslateInfoBarDelegate2::CreateInstance(
    Type type,
    TranslateErrors::Type error,
    TabContents* tab_contents,
    const std::string& original_language,
    const std::string& target_language) {
  if (!TranslateManager2::IsSupportedLanguage(original_language) ||
      !TranslateManager2::IsSupportedLanguage(target_language)) {
    return NULL;
  }
  return new TranslateInfoBarDelegate2(type, error, tab_contents,
                                       original_language, target_language);
}

TranslateInfoBarDelegate2::TranslateInfoBarDelegate2(
    Type type,
    TranslateErrors::Type error,
    TabContents* tab_contents,
    const std::string& original_language,
    const std::string& target_language)
    : InfoBarDelegate(tab_contents),
      type_(type),
      background_animation_(NONE),
      tab_contents_(tab_contents),
      original_language_index_(-1),
      target_language_index_(-1),
      error_(error),
      infobar_view_(NULL),
      prefs_(tab_contents_->profile()->GetPrefs()) {
  DCHECK((type_ != TRANSLATION_ERROR && error == TranslateErrors::NONE) ||
         (type_ == TRANSLATION_ERROR && error != TranslateErrors::NONE));

  std::vector<std::string> language_codes;
  TranslateManager2::GetSupportedLanguages(&language_codes);

  languages_.reserve(language_codes.size());
  for (std::vector<std::string>::const_iterator iter = language_codes.begin();
       iter != language_codes.end(); ++iter) {
    std::string language_code = *iter;

    if (language_code == original_language)
      original_language_index_ = iter - language_codes.begin();
    else if (language_code == target_language)
      target_language_index_ = iter - language_codes.begin();

    string16 language_name = GetLanguageDisplayableName(language_code);
    // Insert the language in languages_ in alphabetical order.
    std::vector<LanguageNamePair>::iterator iter2;
    for (iter2 = languages_.begin(); iter2 != languages_.end(); ++iter2) {
      if (language_name.compare(iter2->second) < 0)
        break;
    }
    languages_.insert(iter2, LanguageNamePair(language_code, language_name));
  }
  DCHECK(original_language_index_ != -1);
  DCHECK(target_language_index_ != -1);
}

int TranslateInfoBarDelegate2::GetLanguageCount() const {
  return static_cast<int>(languages_.size());
}

const std::string& TranslateInfoBarDelegate2::GetLanguageCodeAt(
    int index) const {
  DCHECK(index >=0 && index < GetLanguageCount());
  return languages_[index].first;
}

const string16& TranslateInfoBarDelegate2::GetLanguageDisplayableNameAt(
    int index) const {
  DCHECK(index >=0 && index < GetLanguageCount());
  return languages_[index].second;
}

const std::string& TranslateInfoBarDelegate2::GetOriginalLanguageCode() const {
  return GetLanguageCodeAt(original_language_index());
}

const std::string& TranslateInfoBarDelegate2::GetTargetLanguageCode() const {
  return GetLanguageCodeAt(target_language_index());
}

void TranslateInfoBarDelegate2::SetOriginalLanguage(int language_index) {
  DCHECK(language_index < static_cast<int>(languages_.size()));
  original_language_index_ = language_index;
  if (infobar_view_)
    infobar_view_->OriginalLanguageChanged();
  if (type_ == AFTER_TRANSLATE)
    Translate();
}

void TranslateInfoBarDelegate2::SetTargetLanguage(int language_index) {
  DCHECK(language_index < static_cast<int>(languages_.size()));
  target_language_index_ = language_index;
  if (infobar_view_)
    infobar_view_->TargetLanguageChanged();
  if (type_ == AFTER_TRANSLATE)
    Translate();
}

bool TranslateInfoBarDelegate2::IsError() {
  return type_ == TRANSLATION_ERROR;
}

void TranslateInfoBarDelegate2::Translate() {
  Singleton<TranslateManager2>::get()->TranslatePage(
      tab_contents_,
      GetLanguageCodeAt(original_language_index()),
      GetLanguageCodeAt(target_language_index()));
}

void TranslateInfoBarDelegate2::RevertTranslation() {
  Singleton<TranslateManager2>::get()->RevertTranslation(tab_contents_);
  tab_contents_->RemoveInfoBar(this);
}

void TranslateInfoBarDelegate2::TranslationDeclined() {
  // Remember that the user declined the translation so as to prevent showing a
  // translate infobar for that page again.  (TranslateManager initiates
  // translations when getting a LANGUAGE_DETERMINED from the page, which
  // happens when a load stops. That could happen multiple times, including
  // after the user already declined the translation.)
  tab_contents_->language_state().set_translation_declined(true);
}

void TranslateInfoBarDelegate2::InfoBarDismissed() {
  if (type_ != BEFORE_TRANSLATE)
    return;

  // The user closed the infobar without clicking the translate button.
  TranslationDeclined();
  UMA_HISTOGRAM_COUNTS("Translate.DeclineTranslateCloseInfobar", 1);
}

SkBitmap* TranslateInfoBarDelegate2::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_TRANSLATE);
}

InfoBarDelegate::Type TranslateInfoBarDelegate2::GetInfoBarType() {
  return InfoBarDelegate::PAGE_ACTION_TYPE;
}

bool TranslateInfoBarDelegate2::IsLanguageBlacklisted() {
  const std::string& original_lang =
      GetLanguageCodeAt(original_language_index());
  return prefs_.IsLanguageBlacklisted(original_lang);
}

void TranslateInfoBarDelegate2::ToggleLanguageBlacklist() {
  const std::string& original_lang =
      GetLanguageCodeAt(original_language_index());
  if (prefs_.IsLanguageBlacklisted(original_lang)) {
    prefs_.RemoveLanguageFromBlacklist(original_lang);
  } else {
    prefs_.BlacklistLanguage(original_lang);
    tab_contents_->RemoveInfoBar(this);
  }
}

bool TranslateInfoBarDelegate2::IsSiteBlacklisted() {
  std::string host = GetPageHost();
  return !host.empty() && prefs_.IsSiteBlacklisted(host);
}

void TranslateInfoBarDelegate2::ToggleSiteBlacklist() {
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

bool TranslateInfoBarDelegate2::ShouldAlwaysTranslate() {
  return prefs_.IsLanguagePairWhitelisted(GetOriginalLanguageCode(),
                                          GetTargetLanguageCode());
}

void TranslateInfoBarDelegate2::ToggleAlwaysTranslate() {
  std::string original_lang = GetOriginalLanguageCode();
  std::string target_lang = GetTargetLanguageCode();
  if (prefs_.IsLanguagePairWhitelisted(original_lang, target_lang))
    prefs_.RemoveLanguagePairFromWhitelist(original_lang, target_lang);
  else
    prefs_.WhitelistLanguagePair(original_lang, target_lang);
}

string16 TranslateInfoBarDelegate2::GetMessageInfoBarText() {
  switch (type_) {
    case TRANSLATING:
      return l10n_util::GetStringFUTF16(
          IDS_TRANSLATE_INFOBAR_TRANSLATING_TO,
          GetLanguageDisplayableNameAt(target_language_index_));
    case TRANSLATION_ERROR:
      switch (error_) {
        case TranslateErrors::NETWORK:
          return l10n_util::GetStringUTF16(
              IDS_TRANSLATE_INFOBAR_ERROR_CANT_CONNECT);
        case TranslateErrors::INITIALIZATION_ERROR:
        case TranslateErrors::TRANSLATION_ERROR:
          return l10n_util::GetStringUTF16(
              IDS_TRANSLATE_INFOBAR_ERROR_CANT_TRANSLATE);
        default:
          NOTREACHED();
          return string16();
      }
    default:
      NOTREACHED();
      return string16();
  }
}

string16 TranslateInfoBarDelegate2::GetMessageInfoBarButtonText() {
  switch (type_) {
    case TRANSLATING:
      return string16();
    case TRANSLATION_ERROR:
      return l10n_util::GetStringUTF16(IDS_TRANSLATE_INFOBAR_RETRY);
    default:
      NOTREACHED();
      return string16();
  }
}

void TranslateInfoBarDelegate2::MessageInfoBarButtonPressed() {
  DCHECK(type_ == TRANSLATION_ERROR);
  Singleton<TranslateManager2>::get()->TranslatePage(
      tab_contents_,
      GetLanguageCodeAt(original_language_index()),
      GetLanguageCodeAt(target_language_index()));
}

void TranslateInfoBarDelegate2::UpdateBackgroundAnimation(
    TranslateInfoBarDelegate2* previous_infobar) {
  if (!previous_infobar || previous_infobar->IsError() == IsError()) {
    background_animation_ = NONE;
    return;
  }
  background_animation_ = IsError() ? NORMAL_TO_ERROR : ERROR_TO_NORMAL;
}

std::string TranslateInfoBarDelegate2::GetPageHost() {
  NavigationEntry* entry = tab_contents_->controller().GetActiveEntry();
  return entry ? entry->url().HostNoBrackets() : std::string();
}

// static
string16 TranslateInfoBarDelegate2::GetLanguageDisplayableName(
    const std::string& language_code) {
  return l10n_util::GetDisplayNameForLocale(
      language_code, g_browser_process->GetApplicationLocale(), true);
}

// static
void TranslateInfoBarDelegate2::GetAfterTranslateStrings(
    std::vector<string16>* strings, bool* swap_languages) {
  DCHECK(strings);
  DCHECK(swap_languages);

  std::vector<size_t> offsets;
  string16 text =
      l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_AFTER_MESSAGE,
                                 string16(), string16(), &offsets);
  DCHECK(offsets.size() == 2U);

  if (offsets[0] > offsets[1]) {
    // Target language comes before source.
    std::swap(offsets[0], offsets[1]);
    *swap_languages = true;
  } else {
    *swap_languages = false;
  }

  strings->push_back(text.substr(0, offsets[0]));
  strings->push_back(text.substr(offsets[0], offsets[1]));
  strings->push_back(text.substr(offsets[1]));
}

#if !defined(OS_WIN) && !defined(OS_CHROMEOS)
// Necessary so we link OK on Mac and Linux while the new translate infobars
// are being ported to these platforms.
InfoBar* TranslateInfoBarDelegate2::CreateInfoBar() {
  return NULL;
}
#endif
