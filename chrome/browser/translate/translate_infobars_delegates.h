// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBARS_DELEGATES_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBARS_DELEGATES_H_

#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/translate/translate_prefs.h"

class SkBitmap;

// An interface derived from InfoBarDelegate to form the base interface for
// translate infobars.
class TranslateInfoBarDelegate : public InfoBarDelegate {
 public:
  virtual void GetAvailableOriginalLanguages(
      std::vector<std::string>* languages);
  virtual void GetAvailableTargetLanguages(
      std::vector<std::string>* languages);
  virtual void ModifyOriginalLanguage(int lang_index);
  virtual void ModifyTargetLanguage(int lang_index);
  virtual void Translate();
  virtual bool IsLanguageBlacklisted();
  virtual void ToggleLanguageBlacklist();
  virtual bool IsSiteBlacklisted();
  virtual void ToggleSiteBlacklist();
  virtual bool ShouldAlwaysTranslate();
  virtual void ToggleAlwaysTranslate();

  const std::string& original_lang_code() const {
    return supported_languages_[original_lang_index_];
  }
  const std::string& target_lang_code() const {
    return supported_languages_[target_lang_index_];
  }
  const std::string& GetLocaleFromIndex(int lang_index) const {
    return supported_languages_[lang_index];
  }
  TabContents* tab_contents() const {
    return tab_contents_;
  }

  // Overridden from InfoBarDelegate.
  virtual Type GetInfoBarType() {
    return PAGE_ACTION_TYPE;
  }
  virtual SkBitmap* GetIcon() const;
  virtual bool EqualsDelegate(InfoBarDelegate* delegate) const;
  virtual TranslateInfoBarDelegate* AsTranslateInfoBarDelegate() {
    return this;
  }
  virtual void InfoBarClosed();

  // Returns the printable version of the language code |language_code|.
  static string16 GetDisplayNameForLocale(const std::string& language_code);

 protected:
  TranslateInfoBarDelegate(TabContents* contents, PrefService* user_prefs,
      const std::string& original_language, const std::string& target_language);

  TabContents* tab_contents_;  // Weak.
  int original_lang_index_;
  int target_lang_index_;
  TranslatePrefs prefs_;
  // The list of languages supported.
  std::vector<std::string> supported_languages_;

  DISALLOW_COPY_AND_ASSIGN(TranslateInfoBarDelegate);
};

class BeforeTranslateInfoBarDelegate : public TranslateInfoBarDelegate {
 public:
  BeforeTranslateInfoBarDelegate(TabContents* contents, PrefService* user_prefs,
      const GURL& url, const std::string& original_language,
      const std::string& target_language);

  // Overriden from TranslateInfoBarDelegate:
  virtual bool IsLanguageBlacklisted();
  virtual void ToggleLanguageBlacklist();
  virtual bool IsSiteBlacklisted();
  virtual void ToggleSiteBlacklist();

  // Overridden from InfoBarDelegate:
  virtual InfoBar* CreateInfoBar();

 private:
  std::string site_;
  bool never_translate_language_;
  bool never_translate_site_;

  DISALLOW_COPY_AND_ASSIGN(BeforeTranslateInfoBarDelegate);
};

class AfterTranslateInfoBarDelegate : public TranslateInfoBarDelegate {
 public:
  AfterTranslateInfoBarDelegate(TabContents* contents, PrefService* user_prefs,
      const std::string& original_language, const std::string& target_language);

  // Overriden from TranslateInfoBar:
  virtual void GetAvailableTargetLanguages(
      std::vector<std::string>* languages);
  virtual void ModifyTargetLanguage(int lang_index);
  virtual bool ShouldAlwaysTranslate();
  virtual void ToggleAlwaysTranslate();

  // Overridden from InfoBarDelegate:
  virtual InfoBar* CreateInfoBar();

 private:
  bool always_translate_;

  DISALLOW_COPY_AND_ASSIGN(AfterTranslateInfoBarDelegate);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBARS_DELEGATES_H_
