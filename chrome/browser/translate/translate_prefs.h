// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_PREFS_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_PREFS_H_

#include <string>

#include "googleurl/src/gurl.h"

class PrefRegistrySyncable;
class PrefService;

namespace base {
class DictionaryValue;
class ListValue;
}

class TranslatePrefs {
 public:
  static const char kPrefTranslateLanguageBlacklist[];
  static const char kPrefTranslateSiteBlacklist[];
  static const char kPrefTranslateWhitelists[];
  static const char kPrefTranslateDeniedCount[];
  static const char kPrefTranslateAcceptedCount[];

  explicit TranslatePrefs(PrefService* user_prefs);

  bool IsLanguageBlacklisted(const std::string& original_language) const;
  void BlacklistLanguage(const std::string& original_language);
  void RemoveLanguageFromBlacklist(const std::string& original_language);

  bool IsSiteBlacklisted(const std::string& site) const;
  void BlacklistSite(const std::string& site);
  void RemoveSiteFromBlacklist(const std::string& site);

  bool IsLanguagePairWhitelisted(const std::string& original_language,
      const std::string& target_language);
  void WhitelistLanguagePair(const std::string& original_language,
      const std::string& target_language);
  void RemoveLanguagePairFromWhitelist(const std::string& original_language,
      const std::string& target_language);

  // Will return true if at least one language has been blacklisted.
  bool HasBlacklistedLanguages() const;
  void ClearBlacklistedLanguages();

  // Will return true if at least one site has been blacklisted.
  bool HasBlacklistedSites();
  void ClearBlacklistedSites();

  // These methods are used to track how many times the user has denied the
  // translation for a specific language. (So we can present a UI to black-list
  // that language if the user keeps denying translations).
  int GetTranslationDeniedCount(const std::string& language) const;
  void IncrementTranslationDeniedCount(const std::string& language);
  void ResetTranslationDeniedCount(const std::string& language);

  // These methods are used to track how many times the user has accepted the
  // translation for a specific language. (So we can present a UI to white-list
  // that language if the user keeps accepting translations).
  int GetTranslationAcceptedCount(const std::string& language);
  void IncrementTranslationAcceptedCount(const std::string& language);
  void ResetTranslationAcceptedCount(const std::string& language);

  static bool CanTranslate(PrefService* user_prefs,
      const std::string& original_language, const GURL& url);
  static bool ShouldAutoTranslate(PrefService* user_prefs,
      const std::string& original_language, std::string* target_language);
  static void RegisterUserPrefs(PrefRegistrySyncable* registry);
  static void MigrateUserPrefs(PrefService* user_prefs);

 private:
  bool IsValueBlacklisted(const char* pref_id, const std::string& value) const;
  void BlacklistValue(const char* pref_id, const std::string& value);
  void RemoveValueFromBlacklist(const char* pref_id, const std::string& value);
  bool IsValueInList(
      const base::ListValue* list, const std::string& value) const;
  bool IsLanguageWhitelisted(const std::string& original_language,
      std::string* target_language) const;
  bool IsListEmpty(const char* pref_id) const;
  // Retrieves the dictionary mapping the number of times translation has been
  // denied for a language, creating it if necessary.
  base::DictionaryValue* GetTranslationDeniedCountDictionary();

  // Retrieves the dictionary mapping the number of times translation has been
  // accepted for a language, creating it if necessary.
  base::DictionaryValue* GetTranslationAcceptedCountDictionary() const;

  PrefService* prefs_;  // Weak.
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_PREFS_H_
