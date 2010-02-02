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

void TranslateInfoBarDelegate::InfoBarClosed() {
  delete this;
}

// TranslateInfoBarDelegate: public: -------------------------------------------

void TranslateInfoBarDelegate::ModifyOriginalLanguage(int lang_index) {
  original_lang_index_ = lang_index;
  // TODO(kuan): Send stats to Google Translate that original language has been
  // modified.
}

void TranslateInfoBarDelegate::ModifyTargetLanguage(int lang_index) {
  NOTREACHED() << "Subclass should override";
}

void TranslateInfoBarDelegate::GetAvailableOriginalLanguages(
    std::vector<std::string>* languages) {
  TranslationService::GetSupportedLanguages(languages);
}

void TranslateInfoBarDelegate::GetAvailableTargetLanguages(
    std::vector<std::string>* languages) {
  NOTREACHED() << "Subclass should override";
}

void TranslateInfoBarDelegate::Translate() {
  tab_contents_->TranslatePage(original_lang_code(), target_lang_code());
}

bool TranslateInfoBarDelegate::IsLanguageBlacklisted() {
  NOTREACHED() << "Subclass should override";
  return false;
}

bool TranslateInfoBarDelegate::IsSiteBlacklisted() {
  NOTREACHED() << "Subclass should override";
  return false;
}

bool TranslateInfoBarDelegate::ShouldAlwaysTranslate() {
  NOTREACHED() << "Subclass should override";
  return false;
}

void TranslateInfoBarDelegate::ToggleLanguageBlacklist() {
  NOTREACHED() << "Subclass should override";
}

void TranslateInfoBarDelegate::ToggleSiteBlacklist() {
  NOTREACHED() << "Subclass should override";
}

void TranslateInfoBarDelegate::ToggleAlwaysTranslate() {
  NOTREACHED() << "Subclass should override";
}

// TranslateInfoBarDelegate: static: -------------------------------------------

string16 TranslateInfoBarDelegate::GetDisplayNameForLocale(
    const std::string& language_code) {
  return l10n_util::GetDisplayNameForLocale(
      language_code, g_browser_process->GetApplicationLocale(), true);
}

// TranslateInfoBarDelegate: protected: ----------------------------------------

TranslateInfoBarDelegate::TranslateInfoBarDelegate(TabContents* tab_contents,
    PrefService* user_prefs, const std::string& original_lang_code,
    const std::string& target_lang_code)
    : InfoBarDelegate(tab_contents),
      tab_contents_(tab_contents),
      original_lang_index_(0),
      target_lang_index_(0),
      prefs_(user_prefs) {
  TranslationService::GetSupportedLanguages(&supported_languages_);
  for (size_t i = 0; i < supported_languages_.size(); ++i) {
    if (original_lang_code == supported_languages_[i]) {
      original_lang_index_ = i;
      break;
    }
  }
  for (size_t i = 0; i < supported_languages_.size(); ++i) {
    if (target_lang_code == supported_languages_[i]) {
      target_lang_index_ = i;
      break;
    }
  }
}

// BeforeTranslateInfoBarDelegate: public: -------------------------------------

BeforeTranslateInfoBarDelegate::BeforeTranslateInfoBarDelegate(
    TabContents* tab_contents, PrefService* user_prefs, const GURL& url,
    const std::string& original_lang_code, const std::string& target_lang_code)
    : TranslateInfoBarDelegate(tab_contents, user_prefs, original_lang_code,
          target_lang_code),
      site_(url.HostNoBrackets()),
      never_translate_language_(false),
      never_translate_site_(false) {
}

bool BeforeTranslateInfoBarDelegate::IsLanguageBlacklisted() {
  never_translate_language_ =
    prefs_.IsLanguageBlacklisted(original_lang_code());
  return never_translate_language_;
}

void BeforeTranslateInfoBarDelegate::ToggleLanguageBlacklist() {
  never_translate_language_ = !never_translate_language_;
  if (never_translate_language_)
    prefs_.BlacklistLanguage(original_lang_code());
  else
    prefs_.RemoveLanguageFromBlacklist(original_lang_code());
}

bool BeforeTranslateInfoBarDelegate::IsSiteBlacklisted() {
  never_translate_site_ = prefs_.IsSiteBlacklisted(site_);
  return never_translate_site_;
}

void BeforeTranslateInfoBarDelegate::ToggleSiteBlacklist() {
  never_translate_site_ = !never_translate_site_;
  if (never_translate_site_)
    prefs_.BlacklistSite(site_);
  else
    prefs_.RemoveSiteFromBlacklist(site_);
}

#if !defined(TOOLKIT_VIEWS)
InfoBar* BeforeTranslateInfoBarDelegate::CreateInfoBar() {
  NOTIMPLEMENTED();
  return NULL;
}
#endif  // !TOOLKIT_VIEWS

// AfterTranslateInfoBarDelegate: public: --------------------------------------

AfterTranslateInfoBarDelegate::AfterTranslateInfoBarDelegate(
    TabContents* tab_contents, PrefService* user_prefs,
    const std::string& original_lang_code, const std::string& target_lang_code)
    : TranslateInfoBarDelegate(tab_contents, user_prefs, original_lang_code,
          target_lang_code),
      always_translate_(false) {
}

void AfterTranslateInfoBarDelegate::GetAvailableTargetLanguages(
    std::vector<std::string>* languages) {
  TranslationService::GetSupportedLanguages(languages);
}

void AfterTranslateInfoBarDelegate::ModifyTargetLanguage(int lang_index) {
  target_lang_index_ = lang_index;
}

bool AfterTranslateInfoBarDelegate::ShouldAlwaysTranslate() {
  always_translate_ = prefs_.IsLanguagePairWhitelisted(original_lang_code(),
      target_lang_code());
  return always_translate_;
}

void AfterTranslateInfoBarDelegate::ToggleAlwaysTranslate() {
  always_translate_ = !always_translate_;
  if (always_translate_)
    prefs_.WhitelistLanguagePair(original_lang_code(), target_lang_code());
  else
    prefs_.RemoveLanguagePairFromWhitelist(original_lang_code(),
        target_lang_code());
}

#if !defined(TOOLKIT_VIEWS)
InfoBar* AfterTranslateInfoBarDelegate::CreateInfoBar() {
  NOTIMPLEMENTED();
  return NULL;
}
#endif  // !TOOLKIT_VIEWS
