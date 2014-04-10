// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_infobar_delegate.h"

#include <algorithm>

#include "base/i18n/string_compare.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_manager.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/common/translate_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Counts used to decide whether infobars should be shown.
// Android and iOS implementations do not offer a drop down (for space reasons),
// so we are more aggressive about showing the shortcut to never translate.
// The "Always Translate" option is always shown on iOS and Android.
#if defined(OS_ANDROID)
  const int kAlwaysTranslateMinCount = 1;
  const int kNeverTranslateMinCount = 1;
#elif defined(OS_IOS)
  // The iOS implementation, like the Android implementation, shows the "Never
  // translate" infobar after two denials. There is an offset of one because on
  // Android the last event is not counted.
  const int kAlwaysTranslateMinCount = 1;
  const int kNeverTranslateMinCount = 2;
#else
  const int kAlwaysTranslateMinCount = 3;
  const int kNeverTranslateMinCount = 3;
#endif

}  // namespace

const size_t TranslateInfoBarDelegate::kNoIndex = TranslateUIDelegate::NO_INDEX;

TranslateInfoBarDelegate::~TranslateInfoBarDelegate() {
}

// static
void TranslateInfoBarDelegate::Create(bool replace_existing_infobar,
                                      content::WebContents* web_contents,
                                      translate::TranslateStep step,
                                      const std::string& original_language,
                                      const std::string& target_language,
                                      TranslateErrors::Type error_type,
                                      PrefService* prefs,
                                      bool triggered_from_menu) {
  // Check preconditions.
  if (step != translate::TRANSLATE_STEP_TRANSLATE_ERROR) {
    DCHECK(TranslateDownloadManager::IsSupportedLanguage(target_language));
    if (!TranslateDownloadManager::IsSupportedLanguage(original_language)) {
      // The original language can only be "unknown" for the "translating"
      // infobar, which is the case when the user started a translation from the
      // context menu.
      DCHECK(step == translate::TRANSLATE_STEP_TRANSLATING ||
             step == translate::TRANSLATE_STEP_AFTER_TRANSLATE);
      DCHECK_EQ(translate::kUnknownLanguageCode, original_language);
    }
  }

  // Do not create the after translate infobar if we are auto translating.
  if ((step == translate::TRANSLATE_STEP_AFTER_TRANSLATE) ||
      (step == translate::TRANSLATE_STEP_TRANSLATING)) {
    TranslateTabHelper* translate_tab_helper =
        TranslateTabHelper::FromWebContents(web_contents);
    if (!translate_tab_helper ||
        translate_tab_helper->GetLanguageState().InTranslateNavigation())
      return;
  }

  // Find any existing translate infobar delegate.
  InfoBar* old_infobar = NULL;
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  InfoBarManager* infobar_manager = infobar_service->infobar_manager();
  TranslateInfoBarDelegate* old_delegate = NULL;
  for (size_t i = 0; i < infobar_manager->infobar_count(); ++i) {
    old_infobar = infobar_manager->infobar_at(i);
    old_delegate = old_infobar->delegate()->AsTranslateInfoBarDelegate();
    if (old_delegate) {
      if (!replace_existing_infobar)
        return;
      break;
    }
  }

  // Add the new delegate.
  scoped_ptr<InfoBar> infobar(CreateInfoBar(
      scoped_ptr<TranslateInfoBarDelegate>(new TranslateInfoBarDelegate(
          web_contents, step, old_delegate, original_language,
          target_language, error_type, prefs,
          triggered_from_menu))));
  if (old_delegate)
    infobar_service->ReplaceInfoBar(old_infobar, infobar.Pass());
  else
    infobar_service->AddInfoBar(infobar.Pass());
}


void TranslateInfoBarDelegate::UpdateOriginalLanguageIndex(
    size_t language_index) {
  ui_delegate_.UpdateOriginalLanguageIndex(language_index);
}

void TranslateInfoBarDelegate::UpdateTargetLanguageIndex(
    size_t language_index) {
  ui_delegate_.UpdateTargetLanguageIndex(language_index);
}

void TranslateInfoBarDelegate::Translate() {
  ui_delegate_.Translate();
}

void TranslateInfoBarDelegate::RevertTranslation() {
  ui_delegate_.RevertTranslation();
  infobar()->RemoveSelf();
}

void TranslateInfoBarDelegate::ReportLanguageDetectionError() {
  TranslateManager* manager =
      TranslateTabHelper::GetManagerFromWebContents(web_contents());
  if (!manager)
    return;
  manager->ReportLanguageDetectionError();
}

void TranslateInfoBarDelegate::TranslationDeclined() {
  ui_delegate_.TranslationDeclined(false);
}

bool TranslateInfoBarDelegate::IsTranslatableLanguageByPrefs() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  Profile* original_profile = profile->GetOriginalProfile();
  scoped_ptr<TranslatePrefs> translate_prefs(
      TranslateTabHelper::CreateTranslatePrefs(original_profile->GetPrefs()));
  TranslateAcceptLanguages* accept_languages =
      TranslateTabHelper::GetTranslateAcceptLanguages(original_profile);
  return translate_prefs->CanTranslateLanguage(accept_languages,
                                               original_language_code());
}

void TranslateInfoBarDelegate::ToggleTranslatableLanguageByPrefs() {
  if (ui_delegate_.IsLanguageBlocked()) {
    ui_delegate_.SetLanguageBlocked(false);
  } else {
    ui_delegate_.SetLanguageBlocked(true);
    infobar()->RemoveSelf();
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
    infobar()->RemoveSelf();
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
    infobar()->RemoveSelf();
}

base::string16 TranslateInfoBarDelegate::GetMessageInfoBarText() {
  if (step_ == translate::TRANSLATE_STEP_TRANSLATING) {
    base::string16 target_language_name =
        language_name_at(target_language_index());
    return l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_TRANSLATING_TO,
                                      target_language_name);
  }

  DCHECK_EQ(translate::TRANSLATE_STEP_TRANSLATE_ERROR, step_);
  UMA_HISTOGRAM_ENUMERATION("Translate.ShowErrorInfobar",
                            error_type_,
                            TranslateErrors::TRANSLATE_ERROR_MAX);
  ui_delegate_.OnErrorShown(error_type_);
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
      return base::string16();
  }
}

base::string16 TranslateInfoBarDelegate::GetMessageInfoBarButtonText() {
  if (step_ != translate::TRANSLATE_STEP_TRANSLATE_ERROR) {
    DCHECK_EQ(translate::TRANSLATE_STEP_TRANSLATING, step_);
  } else if ((error_type_ != TranslateErrors::IDENTICAL_LANGUAGES) &&
             (error_type_ != TranslateErrors::UNKNOWN_LANGUAGE)) {
    return l10n_util::GetStringUTF16(
        (error_type_ == TranslateErrors::UNSUPPORTED_LANGUAGE) ?
        IDS_TRANSLATE_INFOBAR_REVERT : IDS_TRANSLATE_INFOBAR_RETRY);
  }
  return base::string16();
}

void TranslateInfoBarDelegate::MessageInfoBarButtonPressed() {
  DCHECK_EQ(translate::TRANSLATE_STEP_TRANSLATE_ERROR, step_);
  if (error_type_ == TranslateErrors::UNSUPPORTED_LANGUAGE) {
    RevertTranslation();
    return;
  }
  // This is the "Try again..." case.
  TranslateManager* manager =
      TranslateTabHelper::GetManagerFromWebContents(web_contents());
  DCHECK(manager);
  manager->TranslatePage(
      original_language_code(), target_language_code(), false);
}

bool TranslateInfoBarDelegate::ShouldShowMessageInfoBarButton() {
  return !GetMessageInfoBarButtonText().empty();
}

bool TranslateInfoBarDelegate::ShouldShowNeverTranslateShortcut() {
  DCHECK_EQ(translate::TRANSLATE_STEP_BEFORE_TRANSLATE, step_);
  return !web_contents()->GetBrowserContext()->IsOffTheRecord() &&
      (prefs_->GetTranslationDeniedCount(original_language_code()) >=
          kNeverTranslateMinCount);
}

bool TranslateInfoBarDelegate::ShouldShowAlwaysTranslateShortcut() {
  DCHECK_EQ(translate::TRANSLATE_STEP_BEFORE_TRANSLATE, step_);
  return !web_contents()->GetBrowserContext()->IsOffTheRecord() &&
      (prefs_->GetTranslationAcceptedCount(original_language_code()) >=
          kAlwaysTranslateMinCount);
}

// static
base::string16 TranslateInfoBarDelegate::GetLanguageDisplayableName(
    const std::string& language_code) {
  return l10n_util::GetDisplayNameForLocale(
      language_code, g_browser_process->GetApplicationLocale(), true);
}

// static
void TranslateInfoBarDelegate::GetAfterTranslateStrings(
    std::vector<base::string16>* strings,
    bool* swap_languages,
    bool autodetermined_source_language) {
  DCHECK(strings);

  if (autodetermined_source_language) {
    size_t offset;
    base::string16 text = l10n_util::GetStringFUTF16(
        IDS_TRANSLATE_INFOBAR_AFTER_MESSAGE_AUTODETERMINED_SOURCE_LANGUAGE,
        base::string16(),
        &offset);

    strings->push_back(text.substr(0, offset));
    strings->push_back(text.substr(offset));
    return;
  }
  DCHECK(swap_languages);

  std::vector<size_t> offsets;
  base::string16 text = l10n_util::GetStringFUTF16(
      IDS_TRANSLATE_INFOBAR_AFTER_MESSAGE, base::string16(), base::string16(),
      &offsets);
  DCHECK_EQ(2U, offsets.size());

  *swap_languages = (offsets[0] > offsets[1]);
  if (*swap_languages)
    std::swap(offsets[0], offsets[1]);

  strings->push_back(text.substr(0, offsets[0]));
  strings->push_back(text.substr(offsets[0], offsets[1] - offsets[0]));
  strings->push_back(text.substr(offsets[1]));
}

TranslateInfoBarDelegate::TranslateInfoBarDelegate(
    content::WebContents* web_contents,
    translate::TranslateStep step,
    TranslateInfoBarDelegate* old_delegate,
    const std::string& original_language,
    const std::string& target_language,
    TranslateErrors::Type error_type,
    PrefService* prefs,
    bool triggered_from_menu)
    : InfoBarDelegate(),
      step_(step),
      background_animation_(NONE),
      ui_delegate_(TranslateTabHelper::FromWebContents(web_contents),
                   TranslateTabHelper::GetManagerFromWebContents(web_contents),
                   original_language,
                   target_language),
      error_type_(error_type),
      prefs_(TranslateTabHelper::CreateTranslatePrefs(prefs)),
      triggered_from_menu_(triggered_from_menu) {
  DCHECK_NE((step_ == translate::TRANSLATE_STEP_TRANSLATE_ERROR),
            (error_type_ == TranslateErrors::NONE));

  if (old_delegate && (old_delegate->is_error() != is_error()))
    background_animation_ = is_error() ? NORMAL_TO_ERROR : ERROR_TO_NORMAL;
}

// TranslateInfoBarDelegate::CreateInfoBar() is implemented in platform-specific
// files.

void TranslateInfoBarDelegate::InfoBarDismissed() {
  if (step_ != translate::TRANSLATE_STEP_BEFORE_TRANSLATE)
    return;

  // The user closed the infobar without clicking the translate button.
  TranslationDeclined();
  UMA_HISTOGRAM_BOOLEAN("Translate.DeclineTranslateCloseInfobar", true);
}

int TranslateInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_TRANSLATE;
}

InfoBarDelegate::Type TranslateInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

bool TranslateInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  // Note: we allow closing this infobar even if the main frame navigation
  // was programmatic and not initiated by the user - crbug.com/70261 .
  if (!details.is_navigation_to_different_page && !details.is_main_frame)
    return false;

  return InfoBarDelegate::ShouldExpireInternal(details);
}

TranslateInfoBarDelegate*
    TranslateInfoBarDelegate::AsTranslateInfoBarDelegate() {
  return this;
}
