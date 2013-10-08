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
#include "components/translate/common/translate_constants.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"

using content::NavigationEntry;

namespace {

// TODO(hajimehoshi): Remove this after crbug/283720 is solved.
const char kDeclineTranslate[] = "Translate.DeclineTranslate";

const char kCloseInfobar[] = "Translate.DeclineTranslateCloseInfobar";
const char kShowErrorInfobar[] = "Translate.ShowErrorInfobar";

}  // namespace

const size_t TranslateInfoBarDelegate::kNoIndex = TranslateUIDelegate::NO_INDEX;

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
  ui_delegate_.Translate();
}

void TranslateInfoBarDelegate::RevertTranslation() {
  ui_delegate_.RevertTranslation();
  RemoveSelf();
}

void TranslateInfoBarDelegate::ReportLanguageDetectionError() {
  TranslateManager::GetInstance()->ReportLanguageDetectionError(
      web_contents());
}

void TranslateInfoBarDelegate::TranslationDeclined() {
  // TODO(miguelg) switch this back to just calling web_contents()
  // once we've diagnosed crbug/283720
  // TODO(hajimehoshi): Replace this implementstion with calling ui_delegate_.
  // TranslationDeclined() after miguelg's investigating.
  content::WebContents* contents = owner()->web_contents();
  content::BrowserContext* context = contents->GetBrowserContext();
  if (!context->IsOffTheRecord()) {
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
  if (ui_delegate_.IsLanguageBlocked()) {
    ui_delegate_.SetLanguageBlocked(false);
  } else {
    ui_delegate_.SetLanguageBlocked(true);
    RemoveSelf();
  }
}

bool TranslateInfoBarDelegate::IsSiteBlacklisted() {
  return ui_delegate_.IsSiteBlacklisted();
}

void TranslateInfoBarDelegate::ToggleSiteBlacklist() {
  if (ui_delegate_.IsSiteBlacklisted()) {
    ui_delegate_.SetSiteBlacklist(false);
  } else {
    ui_delegate_.SetSiteBlacklist(true);
    RemoveSelf();
  }
}

bool TranslateInfoBarDelegate::ShouldAlwaysTranslate() {
  return ui_delegate_.ShouldAlwaysTranslate();
}

void TranslateInfoBarDelegate::ToggleAlwaysTranslate() {
  ui_delegate_.SetAlwaysTranslate(!ui_delegate_.ShouldAlwaysTranslate());
}

void TranslateInfoBarDelegate::AlwaysTranslatePageLanguage() {
  DCHECK(!ui_delegate_.ShouldAlwaysTranslate());
  ui_delegate_.SetAlwaysTranslate(true);
  Translate();
}

void TranslateInfoBarDelegate::NeverTranslatePageLanguage() {
  DCHECK(!ui_delegate_.IsLanguageBlocked());
  ui_delegate_.SetLanguageBlocked(true);
  RemoveSelf();
}

string16 TranslateInfoBarDelegate::GetMessageInfoBarText() {
  if (infobar_type_ == TRANSLATING) {
    string16 target_language_name = language_name_at(target_language_index());
    return l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_TRANSLATING_TO,
                                      target_language_name);
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
          language_name_at(target_language_index()));
    case TranslateErrors::IDENTICAL_LANGUAGES:
      return l10n_util::GetStringFUTF16(
          IDS_TRANSLATE_INFOBAR_ERROR_SAME_LANGUAGE,
          language_name_at(target_language_index()));
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
      ui_delegate_(web_contents(), original_language, target_language),
      error_type_(error_type),
      prefs_(prefs),
      shortcut_config_(shortcut_config) {
  DCHECK_NE((infobar_type_ == TRANSLATION_ERROR),
            (error_type_ == TranslateErrors::NONE));

  if (old_delegate && (old_delegate->is_error() != is_error()))
    background_animation_ = is_error() ? NORMAL_TO_ERROR : ERROR_TO_NORMAL;
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
