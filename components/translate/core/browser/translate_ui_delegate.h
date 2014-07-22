// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_UI_DELEGATE_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_UI_DELEGATE_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/translate/core/common/translate_errors.h"

namespace translate {

class LanguageState;
class TranslateClient;
class TranslateDriver;
class TranslateManager;
class TranslatePrefs;

// The TranslateUIDelegate is a generic delegate for UI which offers Translate
// feature to the user.
class TranslateUIDelegate {
 public:
  static const size_t kNoIndex = static_cast<size_t>(-1);

  TranslateUIDelegate(const base::WeakPtr<TranslateManager>& translate_manager,
                      const std::string& original_language,
                      const std::string& target_language);
  virtual ~TranslateUIDelegate();

  // Handles when an error message is shown.
  void OnErrorShown(TranslateErrors::Type error_type);

  // Returns the LanguageState associated with this object.
  const LanguageState& GetLanguageState();

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
  base::string16 GetLanguageNameAt(size_t index) const;

  // The original language for Translate.
  std::string GetOriginalLanguageCode() const;

  // The target language for Translate.
  std::string GetTargetLanguageCode() const;

  // Starts translating the current page.
  void Translate();

  // Reverts translation.
  void RevertTranslation();

  // Processes when the user declines translation.
  void TranslationDeclined(bool explicitly_closed);

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
  // Gets the host of the page being translated, or an empty string if no URL is
  // associated with the current page.
  std::string GetPageHost();

  TranslateDriver* translate_driver_;
  base::WeakPtr<TranslateManager> translate_manager_;

  typedef std::pair<std::string, base::string16> LanguageNamePair;

  // The list supported languages for translation.
  // The pair first string is the language ISO code (ex: en, fr...), the second
  // string is the displayable name on the current locale.
  // The languages are sorted alphabetically based on the displayable name.
  std::vector<LanguageNamePair> languages_;

  // The index for language the page is originally in.
  size_t original_language_index_;

  // The index for language the page is originally in that was originally
  // reported (original_language_index_ changes if the user selects a new
  // original language, but this one does not).  This is necessary to report
  // language detection errors with the right original language even if the user
  // changed the original language.
  size_t initial_original_language_index_;

  // The index for language the page should be translated to.
  size_t target_language_index_;

  // The translation related preferences.
  scoped_ptr<TranslatePrefs> prefs_;

  DISALLOW_COPY_AND_ASSIGN(TranslateUIDelegate);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_UI_DELEGATE_H_
