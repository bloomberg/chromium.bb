// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_infobar_delegate.h"

#include <algorithm>

#include "base/i18n/string_compare.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_accept_languages.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"

using content::NavigationEntry;

namespace {

const char kDeclineTranslate[] = "Translate.DeclineTranslate";
const char kCloseInfobar[] = "Translate.DeclineTranslateCloseInfobar";
const char kRevertTranslation[] = "Translate.RevertTranslation";
const char kShowErrorInfobar[] = "Translate.ShowErrorInfobar";
const char kPerformTranslate[] = "Translate.Translate";
const char kNeverTranslateLang[] = "Translate.NeverTranslateLang";
const char kNeverTranslateSite[] = "Translate.NeverTranslateSite";
const char kAlwaysTranslateLang[] = "Translate.AlwaysTranslateLang";

}  // namespace

// static
const size_t TranslateInfoBarDelegate::kNoIndex = static_cast<size_t>(-1);

TranslateInfoBarDelegate::~TranslateInfoBarDelegate() {
}

// static
void TranslateInfoBarDelegate::Create(
    bool replace_existing_infobar,
    InfoBarService* infobar_service,
    Type infobar_type,
    const std::string& original_language,
    const std::string& target_language,
    TranslateErrors::Type error_type,
    PrefService* prefs,
    const ShortcutConfiguration& shortcut_config) {
  // Check preconditions.
  if (infobar_type != TRANSLATION_ERROR) {
    DCHECK(TranslateManager::IsSupportedLanguage(target_language));
    if (!TranslateManager::IsSupportedLanguage(original_language)) {
      // The original language can only be "unknown" for the "translating"
      // infobar, which is the case when the user started a translation from the
      // context menu.
      DCHECK(infobar_type == TRANSLATING || infobar_type == AFTER_TRANSLATE);
      DCHECK_EQ(translate::kUnknownLanguageCode, original_language);
    }
  }

  // Find any existing translate infobar delegate.
  TranslateInfoBarDelegate* old_delegate = NULL;
  for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
    old_delegate = infobar_service->infobar_at(i)->AsTranslateInfoBarDelegate();
    if (old_delegate) {
      if (!replace_existing_infobar)
        return;
      break;
    }
  }

  // Create the new delegate.
  scoped_ptr<TranslateInfoBarDelegate> infobar(
      new TranslateInfoBarDelegate(infobar_service, infobar_type, old_delegate,
                                   original_language, target_language,
                                   error_type, prefs, shortcut_config));

  // Do not create the after translate infobar if we are auto translating.
  if ((infobar_type == TranslateInfoBarDelegate::AFTER_TRANSLATE) ||
      (infobar_type == TranslateInfoBarDelegate::TRANSLATING)) {
    TranslateTabHelper* translate_tab_helper =
      TranslateTabHelper::FromWebContents(infobar_service->web_contents());
    if (!translate_tab_helper ||
         translate_tab_helper->language_state().InTranslateNavigation())
      return;
  }

  // Add the new delegate if necessary.
  if (!old_delegate) {
    infobar_service->AddInfoBar(infobar.PassAs<InfoBarDelegate>());
  } else {
    DCHECK(replace_existing_infobar);
    infobar_service->ReplaceInfoBar(old_delegate,
                                    infobar.PassAs<InfoBarDelegate>());
  }
}

void TranslateInfoBarDelegate::Translate() {
  if (!web_contents()->GetBrowserContext()->IsOffTheRecord()) {
    prefs_.ResetTranslationDeniedCount(original_language_code());
    prefs_.IncrementTranslationAcceptedCount(original_language_code());
  }

  TranslateManager::GetInstance()->TranslatePage(web_contents(),
                                                 original_language_code(),
                                                 target_language_code());

  UMA_HISTOGRAM_BOOLEAN(kPerformTranslate, true);
}

void TranslateInfoBarDelegate::RevertTranslation() {
  TranslateManager::GetInstance()->RevertTranslation(web_contents());
  RemoveSelf();

  UMA_HISTOGRAM_BOOLEAN(kRevertTranslation, true);
}

void TranslateInfoBarDelegate::ReportLanguageDetectionError() {
  TranslateManager::GetInstance()->ReportLanguageDetectionError(
      web_contents());
}

void TranslateInfoBarDelegate::TranslationDeclined() {
  if (!web_contents()->GetBrowserContext()->IsOffTheRecord()) {
    prefs_.ResetTranslationAcceptedCount(original_language_code());
    prefs_.IncrementTranslationDeniedCount(original_language_code());
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

bool TranslateInfoBarDelegate::IsTranslatableLanguageByPrefs() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  Profile* original_profile = profile->GetOriginalProfile();
  return TranslatePrefs::CanTranslateLanguage(original_profile,
                                              original_language_code());
}

void TranslateInfoBarDelegate::ToggleTranslatableLanguageByPrefs() {
  const std::string& original_lang = original_language_code();
  if (prefs_.IsBlockedLanguage(original_lang)) {
    prefs_.UnblockLanguage(original_lang);
  } else {
    prefs_.BlockLanguage(original_lang);
    RemoveSelf();
  }

  UMA_HISTOGRAM_BOOLEAN(kNeverTranslateLang, true);
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

  UMA_HISTOGRAM_BOOLEAN(kNeverTranslateSite, true);
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

  UMA_HISTOGRAM_BOOLEAN(kAlwaysTranslateLang, true);
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
  prefs_.BlockLanguage(original_lang);
  RemoveSelf();
}

string16 TranslateInfoBarDelegate::GetMessageInfoBarText() {
  if (infobar_type_ == TRANSLATING) {
    return l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_TRANSLATING_TO,
                                      language_name_at(target_language_index_));
  }

  DCHECK_EQ(TRANSLATION_ERROR, infobar_type_);
  UMA_HISTOGRAM_ENUMERATION(kShowErrorInfobar,
                            error_type_,
                            TranslateErrors::TRANSLATE_ERROR_MAX);
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
  TranslateManager::GetInstance()->TranslatePage(
      web_contents(), original_language_code(), target_language_code());
}

bool TranslateInfoBarDelegate::ShouldShowMessageInfoBarButton() {
  return !GetMessageInfoBarButtonText().empty();
}

bool TranslateInfoBarDelegate::ShouldShowNeverTranslateShortcut() {
  DCHECK_EQ(BEFORE_TRANSLATE, infobar_type_);
  return !web_contents()->GetBrowserContext()->IsOffTheRecord() &&
      (prefs_.GetTranslationDeniedCount(original_language_code()) >=
          shortcut_config_.never_translate_min_count);
}

bool TranslateInfoBarDelegate::ShouldShowAlwaysTranslateShortcut() {
  DCHECK_EQ(BEFORE_TRANSLATE, infobar_type_);
  return !web_contents()->GetBrowserContext()->IsOffTheRecord() &&
      (prefs_.GetTranslationAcceptedCount(original_language_code()) >=
          shortcut_config_.always_translate_min_count);
}

// static
string16 TranslateInfoBarDelegate::GetLanguageDisplayableName(
    const std::string& language_code) {
  return l10n_util::GetDisplayNameForLocale(
      language_code, g_browser_process->GetApplicationLocale(), true);
}

// static
void TranslateInfoBarDelegate::GetAfterTranslateStrings(
    std::vector<string16>* strings,
    bool* swap_languages,
    bool autodetermined_source_language) {
  DCHECK(strings);

  if (autodetermined_source_language) {
    size_t offset;
    string16 text = l10n_util::GetStringFUTF16(
        IDS_TRANSLATE_INFOBAR_AFTER_MESSAGE_AUTODETERMINED_SOURCE_LANGUAGE,
        string16(),
        &offset);

    strings->push_back(text.substr(0, offset));
    strings->push_back(text.substr(offset));
    return;
  }
  DCHECK(swap_languages);

  std::vector<size_t> offsets;
  string16 text = l10n_util::GetStringFUTF16(
      IDS_TRANSLATE_INFOBAR_AFTER_MESSAGE, string16(), string16(), &offsets);
  DCHECK_EQ(2U, offsets.size());

  *swap_languages = (offsets[0] > offsets[1]);
  if (*swap_languages)
    std::swap(offsets[0], offsets[1]);

  strings->push_back(text.substr(0, offsets[0]));
  strings->push_back(text.substr(offsets[0], offsets[1] - offsets[0]));
  strings->push_back(text.substr(offsets[1]));
}

TranslateInfoBarDelegate::TranslateInfoBarDelegate(
    InfoBarService* infobar_service,
    Type infobar_type,
    TranslateInfoBarDelegate* old_delegate,
    const std::string& original_language,
    const std::string& target_language,
    TranslateErrors::Type error_type,
    PrefService* prefs,
    ShortcutConfiguration shortcut_config)
    : InfoBarDelegate(infobar_service),
      infobar_type_(infobar_type),
      background_animation_(NONE),
      original_language_index_(kNoIndex),
      initial_original_language_index_(kNoIndex),
      target_language_index_(kNoIndex),
      error_type_(error_type),
      prefs_(prefs),
      shortcut_config_(shortcut_config) {
  DCHECK_NE((infobar_type_ == TRANSLATION_ERROR),
            (error_type_ == TranslateErrors::NONE));

  if (old_delegate && (old_delegate->is_error() != is_error()))
    background_animation_ = is_error() ? NORMAL_TO_ERROR : ERROR_TO_NORMAL;

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

    string16 language_name = GetLanguageDisplayableName(language_code);
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
  DCHECK_NE(kNoIndex, target_language_index_);
}

void TranslateInfoBarDelegate::InfoBarDismissed() {
  if (infobar_type_ != BEFORE_TRANSLATE)
    return;

  // The user closed the infobar without clicking the translate button.
  TranslationDeclined();
  UMA_HISTOGRAM_BOOLEAN(kCloseInfobar, true);
}

int TranslateInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_TRANSLATE;
}

InfoBarDelegate::Type TranslateInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

bool TranslateInfoBarDelegate::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  // Note: we allow closing this infobar even if the main frame navigation
  // was programmatic and not initiated by the user - crbug.com/70261 .
  if (!details.is_navigation_to_different_page() && !details.is_main_frame)
    return false;

  return InfoBarDelegate::ShouldExpireInternal(details);
}

TranslateInfoBarDelegate*
    TranslateInfoBarDelegate::AsTranslateInfoBarDelegate() {
  return this;
}

std::string TranslateInfoBarDelegate::GetPageHost() {
  NavigationEntry* entry = web_contents()->GetController().GetActiveEntry();
  return entry ? entry->GetURL().HostNoBrackets() : std::string();
}
