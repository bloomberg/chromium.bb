// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_UI_DELEGATE_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_UI_DELEGATE_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/common/chrome_constants.h"

class TranslatePrefs;

namespace content {
class WebContents;
}  // namespace content

// The TranslateUIDelegate is a generic delegate for UI which offers Translate
// feature to the user.
//
// Note: TranslateInfobarDelegate should be updated if this implementation is
// updated.
class TranslateUIDelegate {
 public:
  enum {
    NO_INDEX = -1,
  };

  // A pair whose key is a language code and value is the language's
  // displayable name.
  typedef std::pair<std::string, string16> LanguageNamePair;

  TranslateUIDelegate(content::WebContents* web_contents,
                      const std::string& original_language,
                      const std::string& target_language);
  virtual ~TranslateUIDelegate();

  // Retrieves the displayable names of the supported languages, based on
  // |locale|. The returned names are sorted in alphabetical order.
  static std::vector<LanguageNamePair> GetSortedLanguageNames(
      const std::string& locale);

  // Gets the host of the page of |web_contents|, or an empty string if no URL
  // is associated with the current page.
  static std::string GetPageHost(content::WebContents* web_contents);

  content::WebContents* web_contents() { return web_contents_; }

  // Returns the number of languages supported.
  size_t GetNumberOfLanguages() const;

  // Returns the original language index.
  size_t GetOriginalLanguageIndex() const;

  // Updates the original language index.
  void UpdateOriginalLanguageIndex(size_t language_index);

  // Returns the target language index.
  size_t GetTargetLanguageIndex() const;

  // Updates the target language index.
  void UpdateTargetLanguageIndex(size_t language_index);

  // Returns the ISO code for the language at |index|.
  std::string GetLanguageCodeAt(size_t index) const;

  // Returns the displayable name for the language at |index|.
  string16 GetLanguageNameAt(size_t index) const;

  // The original language for Translate.
  std::string GetOriginalLanguageCode() const;

  // The target language for Translate.
  std::string GetTargetLanguageCode() const;

  // Starts translating the current page.
  void Translate();

  // Reverts translation.
  void RevertTranslation();

  // Processes when the user declines translation.
  void TranslationDeclined();

  // Returns true if the current language is blocked.
  bool IsLanguageBlocked();

  // Sets the value if the current language is blocked.
  void SetLanguageBlocked(bool value);

  // Returns true if the current webpage is blacklisted.
  bool IsSiteBlacklisted();

  // Sets the value if the current webpage is blacklisted.
  void SetSiteBlacklist(bool value);

  // Returns true if the webpage in the current original language should be
  // translated into the current target language automatically.
  bool ShouldAlwaysTranslate();

  // Sets the value if the webpage in the current original language should be
  // translated into the current target language automatically.
  void SetAlwaysTranslate(bool value);

 private:
  content::WebContents* web_contents_;

  // The list supported languages for translation.
  // The pair first string is the language ISO code (ex: en, fr...), the second
  // string is the displayable name on the current locale.
  // The languages are sorted alphabetically based on the displayable name.
  std::vector<LanguageNamePair> languages_;

  // The index for language the page is originally in.
  size_t original_language_index_;

  // The index for language the page should be translated to.
  size_t target_language_index_;

  // The translation related preferences.
  scoped_ptr<TranslatePrefs> prefs_;

  DISALLOW_COPY_AND_ASSIGN(TranslateUIDelegate);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_UI_DELEGATE_H_
