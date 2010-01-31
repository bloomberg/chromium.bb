// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_infobars_delegates.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
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

void TranslateInfoBarDelegate::ModifyOriginalLanguage(
    const std::string& original_language) {
  original_language_ = original_language;
  // TODO(kuan): Send stats to Google Translate that original language has been
  // modified.
}

void TranslateInfoBarDelegate::ModifyTargetLanguage(
    const std::string& target_language) {
  NOTREACHED() << "Subclass should override";
}

void TranslateInfoBarDelegate::GetAvailableOriginalLanguages(
    std::vector<std::string>* languages) {
  // TODO(kuan): Call backend when it's ready; hardcode a list for now.
  languages->push_back("Arabic");
  languages->push_back("Bengali");
  languages->push_back("Bulgarian");
  languages->push_back("Chinese (Simplified Han)");
  languages->push_back("Chinese (Traditional Han)");
  languages->push_back("Croatian");
  languages->push_back("Czech");
  languages->push_back("Danish");
  languages->push_back("Dutch");
  languages->push_back("English");
  languages->push_back("Estonian");
  languages->push_back("Filipino");
  languages->push_back("Finnish");
  languages->push_back("French");
  languages->push_back("German");
  languages->push_back("Greek");
  languages->push_back("Hebrew");
  languages->push_back("Hindi");
  languages->push_back("Hungarian");
  languages->push_back("Indonesian");
  languages->push_back("Italian");
  languages->push_back("Japanese");
  languages->push_back("Korean");
  languages->push_back("Latvian");
  languages->push_back("Lithuanian");
  languages->push_back("Norwegian");
  languages->push_back("Polish");
  languages->push_back("Portuguese");
  languages->push_back("Romanian");
  languages->push_back("Russian");
  languages->push_back("Serbian");
  languages->push_back("Slovak");
  languages->push_back("Slovenian");
  languages->push_back("Spanish");
  languages->push_back("Swedish");
  languages->push_back("Tamil");
  languages->push_back("Thai");
  languages->push_back("Turkish");
  languages->push_back("Ukrainian");
  languages->push_back("Vietnamese");
}

void TranslateInfoBarDelegate::GetAvailableTargetLanguages(
    std::vector<std::string>* languages) {
  NOTREACHED() << "Subclass should override";
}

void TranslateInfoBarDelegate::Translate() {
  // TODO(kuan): Call actual Translate method.
/*
  Translate(WideToUTF8(original_language()), WideToUTF8(target_language()));
*/
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

// TranslateInfoBarDelegate: protected: ----------------------------------------

TranslateInfoBarDelegate::TranslateInfoBarDelegate(TabContents* tab_contents,
    PrefService* user_prefs, const std::string& original_language,
    const std::string& target_language)
    : InfoBarDelegate(tab_contents),
      tab_contents_(tab_contents),
      original_language_(original_language),
      target_language_(target_language),
      prefs_(user_prefs) {
}

// BeforeTranslateInfoBarDelegate: public: -------------------------------------

BeforeTranslateInfoBarDelegate::BeforeTranslateInfoBarDelegate(
    TabContents* tab_contents, PrefService* user_prefs, const GURL& url,
    const std::string& original_language, const std::string& target_language)
    : TranslateInfoBarDelegate(tab_contents, user_prefs, original_language,
          target_language),
      site_(url.HostNoBrackets()),
      never_translate_language_(false),
      never_translate_site_(false) {
}

bool BeforeTranslateInfoBarDelegate::IsLanguageBlacklisted() {
  never_translate_language_ =
    prefs_.IsLanguageBlacklisted(original_language());
  return never_translate_language_;
}

void BeforeTranslateInfoBarDelegate::ToggleLanguageBlacklist() {
  never_translate_language_ = !never_translate_language_;
  if (never_translate_language_)
    prefs_.BlacklistLanguage(original_language());
  else
    prefs_.RemoveLanguageFromBlacklist(original_language());
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
    const std::string& original_language, const std::string& target_language)
    : TranslateInfoBarDelegate(tab_contents, user_prefs, original_language,
          target_language),
      always_translate_(false) {
}

void AfterTranslateInfoBarDelegate::GetAvailableTargetLanguages(
    std::vector<std::string>* languages) {
  // TODO(kuan): Call backend when it's ready; hardcode a list for now.
  GetAvailableOriginalLanguages(languages);
}

void AfterTranslateInfoBarDelegate::ModifyTargetLanguage(
    const std::string& target_language) {
  target_language_ = target_language;
}

bool AfterTranslateInfoBarDelegate::ShouldAlwaysTranslate() {
  always_translate_ = prefs_.IsLanguagePairWhitelisted(original_language(),
      target_language());
  return always_translate_;
}

void AfterTranslateInfoBarDelegate::ToggleAlwaysTranslate() {
  always_translate_ = !always_translate_;
  if (always_translate_)
    prefs_.WhitelistLanguagePair(original_language(), target_language());
  else
    prefs_.RemoveLanguagePairFromWhitelist(original_language(),
        target_language());
}

#if !defined(TOOLKIT_VIEWS)
InfoBar* AfterTranslateInfoBarDelegate::CreateInfoBar() {
  NOTIMPLEMENTED();
  return NULL;
}
#endif  // !TOOLKIT_VIEWS
