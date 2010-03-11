// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_infobars_delegates.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_host/translation_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

// TranslateInfoBarDelegate: InfoBarDelegate overrides: ------------------------

SkBitmap* TranslateInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_TRANSLATE);
}

bool TranslateInfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  TranslateInfoBarDelegate* translate_delegate =
      delegate->AsTranslateInfoBarDelegate();
  // There can be only 1 translate infobar at any one time.
  return (!!translate_delegate);
}

void TranslateInfoBarDelegate::InfoBarDismissed() {
  LanguageState& language_state = tab_contents_->language_state();
  if (!language_state.IsPageTranslated() &&
      !language_state.translation_pending()) {
    // The user closed the infobar without clicking the translate button.
    TranslationDeclined();
  }
}

void TranslateInfoBarDelegate::InfoBarClosed() {
  delete this;
}

// TranslateInfoBarDelegate: public: -------------------------------------------

void TranslateInfoBarDelegate::UpdateState(TranslateState new_state) {
  if (state_ != new_state)
    state_ = new_state;
}

void TranslateInfoBarDelegate::ModifyOriginalLanguage(int lang_index) {
  original_lang_index_ = lang_index;
  // TODO(kuan): Send stats to Google Translate that original language has been
  // modified.
}

void TranslateInfoBarDelegate::ModifyTargetLanguage(int lang_index) {
  target_lang_index_ = lang_index;
}

void TranslateInfoBarDelegate::GetAvailableOriginalLanguages(
    std::vector<std::string>* languages) {
  TranslationService::GetSupportedLanguages(languages);
}

void TranslateInfoBarDelegate::GetAvailableTargetLanguages(
    std::vector<std::string>* languages) {
  TranslationService::GetSupportedLanguages(languages);
}

void TranslateInfoBarDelegate::Translate() {
  if (state_ == kBeforeTranslate)
    UpdateState(kTranslating);
  tab_contents_->TranslatePage(original_lang_code(), target_lang_code());
}

void TranslateInfoBarDelegate::TranslationDeclined() {
  // Remember that the user declined the translation so as to prevent showing a
  // translate infobar for that page again.  (TranslateManager initiates
  // translations when getting a LANGUAGE_DETERMINED from the page, which
  // happens when a load stops. That could happen multiple times, including
  // after the user already declined the translation.)
  tab_contents_->language_state().set_translation_declined(true);
}

bool TranslateInfoBarDelegate::IsLanguageBlacklisted() {
  if (state_ == kBeforeTranslate) {
    never_translate_language_ =
        prefs_.IsLanguageBlacklisted(original_lang_code());
    return never_translate_language_;
  }
  NOTREACHED() << "Invalid mehod called for translate state";
  return false;
}

bool TranslateInfoBarDelegate::IsSiteBlacklisted() {
  if (state_ == kBeforeTranslate) {
    never_translate_site_ = prefs_.IsSiteBlacklisted(site_);
    return never_translate_site_;
  }
  NOTREACHED() << "Invalid mehod called for translate state";
  return false;
}

bool TranslateInfoBarDelegate::ShouldAlwaysTranslate() {
  if (state_ == kAfterTranslate) {
    always_translate_ = prefs_.IsLanguagePairWhitelisted(original_lang_code(),
        target_lang_code());
    return always_translate_;
  }
  NOTREACHED() << "Invalid mehod called for translate state";
  return false;
}

void TranslateInfoBarDelegate::ToggleLanguageBlacklist() {
  if (state_ == kBeforeTranslate) {
    never_translate_language_ = !never_translate_language_;
    if (never_translate_language_)
      prefs_.BlacklistLanguage(original_lang_code());
    else
      prefs_.RemoveLanguageFromBlacklist(original_lang_code());
  } else {
    NOTREACHED() << "Invalid method called for translate state";
  }
}

void TranslateInfoBarDelegate::ToggleSiteBlacklist() {
  if (state_ == kBeforeTranslate) {
    never_translate_site_ = !never_translate_site_;
    if (never_translate_site_)
      prefs_.BlacklistSite(site_);
    else
      prefs_.RemoveSiteFromBlacklist(site_);
  } else {
    NOTREACHED() << "Invalid mehod called for translate state";
  }
}

void TranslateInfoBarDelegate::ToggleAlwaysTranslate() {
  if (state_ == kAfterTranslate) {
    always_translate_ = !always_translate_;
    if (always_translate_)
      prefs_.WhitelistLanguagePair(original_lang_code(), target_lang_code());
    else
      prefs_.RemoveLanguagePairFromWhitelist(original_lang_code(),
          target_lang_code());
  } else {
    NOTREACHED() << "Invalid mehod called for translate state";
  }
}

void TranslateInfoBarDelegate::GetMessageText(string16 *message_text,
     std::vector<size_t> *offsets, bool *swapped_language_placeholders) {
  *swapped_language_placeholders = false;
  offsets->clear();

  std::vector<size_t> offsets_tmp;
  int message_resource_id = IDS_TRANSLATE_INFOBAR_BEFORE_MESSAGE;
  if (state() == kAfterTranslate)
    message_resource_id = IDS_TRANSLATE_INFOBAR_AFTER_MESSAGE;
  *message_text = l10n_util::GetStringFUTF16(message_resource_id,
      string16(), string16(), &offsets_tmp);

  if (offsets_tmp.empty() || offsets_tmp.size() > 2) {
    NOTREACHED() << "Invalid no. of placeholders in label.";
    return;
  }
  // Sort the offsets if necessary.
  if (offsets_tmp.size() == 2 && offsets_tmp[0] > offsets_tmp[1]) {
    size_t offset0 = offsets_tmp[0];
    offsets_tmp[0] = offsets_tmp[1];
    offsets_tmp[1] = offset0;
    *swapped_language_placeholders = true;
  }
  if (offsets_tmp[offsets_tmp.size() - 1] != message_text->length())
    offsets_tmp.push_back(message_text->length());
  *offsets = offsets_tmp;
}

// TranslateInfoBarDelegate: static: -------------------------------------------

TranslateInfoBarDelegate* TranslateInfoBarDelegate::Create(
    TabContents* tab_contents, PrefService* user_prefs, TranslateState state,
    const GURL& url,
    const std::string& original_lang_code,
    const std::string& target_lang_code) {
  std::vector<std::string> supported_languages;
  TranslationService::GetSupportedLanguages(&supported_languages);

  int original_lang_index = -1;
  for (size_t i = 0; i < supported_languages.size(); ++i) {
    if (original_lang_code == supported_languages[i]) {
      original_lang_index = i;
      break;
    }
  }
  if (original_lang_index == -1)
    return NULL;

  int target_lang_index = -1;
  for (size_t i = 0; i < supported_languages.size(); ++i) {
    if (target_lang_code == supported_languages[i]) {
      target_lang_index = i;
      break;
    }
  }
  if (target_lang_index == -1)
    return NULL;

  return new TranslateInfoBarDelegate(tab_contents, user_prefs, state, url,
                                      original_lang_index, target_lang_index);
}

string16 TranslateInfoBarDelegate::GetDisplayNameForLocale(
    const std::string& language_code) {
  return l10n_util::GetDisplayNameForLocale(
      language_code, g_browser_process->GetApplicationLocale(), true);
}

// TranslateInfoBarDelegate: private: ------------------------------------------

TranslateInfoBarDelegate::TranslateInfoBarDelegate(TabContents* tab_contents,
    PrefService* user_prefs, TranslateState state, const GURL& url,
    int original_lang_index, int target_lang_index)
    : InfoBarDelegate(tab_contents),
      tab_contents_(tab_contents),
      prefs_(user_prefs),
      state_(state),
      site_(url.HostNoBrackets()),
      original_lang_index_(original_lang_index),
      target_lang_index_(target_lang_index),
      never_translate_language_(false),
      never_translate_site_(false),
      always_translate_(false) {
  TranslationService::GetSupportedLanguages(&supported_languages_);
  DCHECK(original_lang_index_ > -1);
  DCHECK(target_lang_index_ > -1);
}

