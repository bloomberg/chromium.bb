// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_infobar_delegate.h"

#include <algorithm>

#include "base/metrics/histogram.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::NavigationEntry;

// static
const size_t TranslateInfoBarDelegate::kNoIndex = static_cast<size_t>(-1);

// static
TranslateInfoBarDelegate* TranslateInfoBarDelegate::CreateDelegate(
    Type infobar_type,
    InfoBarService* infobar_service,
    PrefService* prefs,
    const std::string& original_language,
    const std::string& target_language) {
  DCHECK_NE(TRANSLATION_ERROR, infobar_type);
  // These must be validated by our callers.
  DCHECK(TranslateManager::IsSupportedLanguage(target_language));
  // The original language can only be "unknown" for the "translating"
  // infobar, which is the case when the user started a translation from the
  // context menu.
  DCHECK(TranslateManager::IsSupportedLanguage(original_language) ||
         ((infobar_type == TRANSLATING) &&
          (original_language == chrome::kUnknownLanguageCode)));
  TranslateInfoBarDelegate* delegate =
      new TranslateInfoBarDelegate(infobar_type,
                                   TranslateErrors::NONE,
                                   infobar_service,
                                   prefs,
                                   original_language,
                                   target_language);
  DCHECK_NE(kNoIndex, delegate->target_language_index());
  return delegate;
}

TranslateInfoBarDelegate* TranslateInfoBarDelegate::CreateErrorDelegate(
    TranslateErrors::Type error_type,
    InfoBarService* infobar_service,
    PrefService* prefs,
    const std::string& original_language,
    const std::string& target_language) {
  return new TranslateInfoBarDelegate(TRANSLATION_ERROR,
                                      error_type,
                                      infobar_service,
                                      prefs,
                                      original_language,
                                      target_language);
}

TranslateInfoBarDelegate::~TranslateInfoBarDelegate() {
}

void TranslateInfoBarDelegate::Translate() {
  if (!owner()->GetWebContents()->GetBrowserContext()->IsOffTheRecord()) {
    prefs_.ResetTranslationDeniedCount(original_language_code());
    prefs_.IncrementTranslationAcceptedCount(original_language_code());
  }

  TranslateManager::GetInstance()->TranslatePage(owner()->GetWebContents(),
                                                 original_language_code(),
                                                 target_language_code());

  UMA_HISTOGRAM_COUNTS("Translate.Translate", 1);
}

void TranslateInfoBarDelegate::RevertTranslation() {
  TranslateManager::GetInstance()->RevertTranslation(owner()->GetWebContents());
  RemoveSelf();
}

void TranslateInfoBarDelegate::ReportLanguageDetectionError() {
  TranslateManager::GetInstance()->
      ReportLanguageDetectionError(owner()->GetWebContents());
}

void TranslateInfoBarDelegate::TranslationDeclined() {
  if (!owner()->GetWebContents()->GetBrowserContext()->IsOffTheRecord()) {
    prefs_.ResetTranslationAcceptedCount(original_language_code());
    prefs_.IncrementTranslationDeniedCount(original_language_code());
  }

  // Remember that the user declined the translation so as to prevent showing a
  // translate infobar for that page again.  (TranslateManager initiates
  // translations when getting a LANGUAGE_DETERMINED from the page, which
  // happens when a load stops. That could happen multiple times, including
  // after the user already declined the translation.)
  TranslateTabHelper* translate_tab_helper =
      TranslateTabHelper::FromWebContents(owner()->GetWebContents());
  translate_tab_helper->language_state().set_translation_declined(true);

  UMA_HISTOGRAM_COUNTS("Translate.DeclineTranslate", 1);
}

bool TranslateInfoBarDelegate::IsLanguageBlacklisted() {
  return prefs_.IsLanguageBlacklisted(original_language_code());
}

void TranslateInfoBarDelegate::ToggleLanguageBlacklist() {
  const std::string& original_lang = original_language_code();
  if (prefs_.IsLanguageBlacklisted(original_lang)) {
    prefs_.RemoveLanguageFromBlacklist(original_lang);
  } else {
    prefs_.BlacklistLanguage(original_lang);
    RemoveSelf();
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
    RemoveSelf();
  }
}

bool TranslateInfoBarDelegate::ShouldAlwaysTranslate() {
  return prefs_.IsLanguagePairWhitelisted(original_language_code(),
                                          target_language_code());
}

void TranslateInfoBarDelegate::ToggleAlwaysTranslate() {
  const std::string& original_lang = original_language_code();
  const std::string& target_lang = target_language_code();
  if (prefs_.IsLanguagePairWhitelisted(original_lang, target_lang))
    prefs_.RemoveLanguagePairFromWhitelist(original_lang, target_lang);
  else
    prefs_.WhitelistLanguagePair(original_lang, target_lang);
}

void TranslateInfoBarDelegate::AlwaysTranslatePageLanguage() {
  const std::string& original_lang = original_language_code();
  const std::string& target_lang = target_language_code();
  DCHECK(!prefs_.IsLanguagePairWhitelisted(original_lang, target_lang));
  prefs_.WhitelistLanguagePair(original_lang, target_lang);
  Translate();
}

void TranslateInfoBarDelegate::NeverTranslatePageLanguage() {
  std::string original_lang = original_language_code();
  DCHECK(!prefs_.IsLanguageBlacklisted(original_lang));
  prefs_.BlacklistLanguage(original_lang);
  RemoveSelf();
}

string16 TranslateInfoBarDelegate::GetMessageInfoBarText() {
  if (infobar_type_ == TRANSLATING) {
    return l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_TRANSLATING_TO,
                                      language_name_at(target_language_index_));
  }

  DCHECK_EQ(TRANSLATION_ERROR, infobar_type_);
  switch (error_type_) {
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
          language_name_at(target_language_index_));
    case TranslateErrors::IDENTICAL_LANGUAGES:
      return l10n_util::GetStringFUTF16(
          IDS_TRANSLATE_INFOBAR_ERROR_SAME_LANGUAGE,
          language_name_at(target_language_index_));
    default:
      NOTREACHED();
      return string16();
  }
}

string16 TranslateInfoBarDelegate::GetMessageInfoBarButtonText() {
  if (infobar_type_ != TRANSLATION_ERROR) {
    DCHECK_EQ(TRANSLATING, infobar_type_);
  } else if ((error_type_ != TranslateErrors::IDENTICAL_LANGUAGES) &&
             (error_type_ != TranslateErrors::UNKNOWN_LANGUAGE)) {
    return l10n_util::GetStringUTF16(
        (error_type_ == TranslateErrors::UNSUPPORTED_LANGUAGE) ?
        IDS_TRANSLATE_INFOBAR_REVERT : IDS_TRANSLATE_INFOBAR_RETRY);
  }
  return string16();
}

void TranslateInfoBarDelegate::MessageInfoBarButtonPressed() {
  DCHECK_EQ(TRANSLATION_ERROR, infobar_type_);
  if (error_type_ == TranslateErrors::UNSUPPORTED_LANGUAGE) {
    RevertTranslation();
    return;
  }
  // This is the "Try again..." case.
  TranslateManager::GetInstance()->TranslatePage(owner()->GetWebContents(),
      original_language_code(), target_language_code());
}

bool TranslateInfoBarDelegate::ShouldShowMessageInfoBarButton() {
  return !GetMessageInfoBarButtonText().empty();
}

bool TranslateInfoBarDelegate::ShouldShowNeverTranslateButton() {
  DCHECK_EQ(BEFORE_TRANSLATE, infobar_type_);
  return !owner()->GetWebContents()->GetBrowserContext()->IsOffTheRecord() &&
      (prefs_.GetTranslationDeniedCount(original_language_code()) >= 3);
}

bool TranslateInfoBarDelegate::ShouldShowAlwaysTranslateButton() {
  DCHECK_EQ(BEFORE_TRANSLATE, infobar_type_);
  return !owner()->GetWebContents()->GetBrowserContext()->IsOffTheRecord() &&
      (prefs_.GetTranslationAcceptedCount(original_language_code()) >= 3);
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
    Type infobar_type,
    TranslateErrors::Type error_type,
    InfoBarService* infobar_service,
    PrefService* prefs,
    const std::string& original_language,
    const std::string& target_language)
    : InfoBarDelegate(infobar_service),
      infobar_type_(infobar_type),
      background_animation_(NONE),
      original_language_index_(kNoIndex),
      initial_original_language_index_(kNoIndex),
      target_language_index_(kNoIndex),
      error_type_(error_type),
      prefs_(prefs) {
  DCHECK_NE((infobar_type == TRANSLATION_ERROR),
            (error_type_ == TranslateErrors::NONE));

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
  DCHECK_NE(kNoIndex, target_language_index_);
}

bool TranslateInfoBarDelegate::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  // Note: we allow closing this infobar even if the main frame navigation
  // was programmatic and not initiated by the user - crbug.com/70261 .
  if (!details.is_navigation_to_different_page() && !details.is_main_frame)
    return false;

  return InfoBarDelegate::ShouldExpireInternal(details);
}

void TranslateInfoBarDelegate::InfoBarDismissed() {
  if (infobar_type_ != BEFORE_TRANSLATE)
    return;

  // The user closed the infobar without clicking the translate button.
  TranslationDeclined();
  UMA_HISTOGRAM_COUNTS("Translate.DeclineTranslateCloseInfobar", 1);
}

gfx::Image* TranslateInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
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
  NavigationEntry* entry =
      owner()->GetWebContents()->GetController().GetActiveEntry();
  return entry ? entry->GetURL().HostNoBrackets() : std::string();
}
