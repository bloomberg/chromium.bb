// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_PREFS_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_PREFS_H_
#pragma once

#include <string>

#include "googleurl/src/gurl.h"

class DictionaryValue;
class ListValue;
class PrefService;

class TranslatePrefs {
 public:
  static const char kPrefTranslateLanguageBlacklist[];
  static const char kPrefTranslateSiteBlacklist[];
  static const char kPrefTranslateWhitelists[];
  static const char kPrefTranslateDeniedCount[];
  static const char kPrefTranslateAcceptedCount[];

  explicit TranslatePrefs(PrefService* user_prefs);

  bool IsLanguageBlacklisted(const std::string& original_language);
  void BlacklistLanguage(const std::string& original_language);
  void RemoveLanguageFromBlacklist(const std::string& original_language);

  bool IsSiteBlacklisted(const std::string& site);
  void BlacklistSite(const std::string& site);
  void RemoveSiteFromBlacklist(const std::string& site);

  bool IsLanguagePairWhitelisted(const std::string& original_language,
      const std::string& target_language);
  void WhitelistLanguagePair(const std::string& original_language,
      const std::string& target_language);
  void RemoveLanguagePairFromWhitelist(const std::string& original_language,
      const std::string& target_language);

  // These methods are used to track how many times the user has denied the
  // translation for a specific language. (So we can present a UI to black-list
  // that language if the user keeps denying translations).
  int GetTranslationDeniedCount(const std::string& language);
  void IncrementTranslationDeniedCount(const std::string& language);
  void ResetTranslationDeniedCount(const std::string& language);

  // These methods are used to track how many times the user has accepted the
  // translation for a specific language. (So we can present a UI to white-list
  // that langueg if the user keeps accepting translations).
  int GetTranslationAcceptedCount(const std::string& language);
  void IncrementTranslationAcceptedCount(const std::string& language);
  void ResetTranslationAcceptedCount(const std::string& language);

  static bool CanTranslate(PrefService* user_prefs,
      const std::string& original_language, const GURL& url);
  static bool ShouldAutoTranslate(PrefService* user_prefs,
      const std::string& original_language, std::string* target_language);
  static void RegisterUserPrefs(PrefService* user_prefs);

 private:
  static void MigrateTranslateWhitelists(PrefService* user_prefs);
  bool IsValueBlacklisted(const char* pref_id, const std::string& value);
  void BlacklistValue(const char* pref_id, const std::string& value);
  void RemoveValueFromBlacklist(const char* pref_id, const std::string& value);
  bool IsValueInList(const ListValue* list, const std::string& value);
  bool IsLanguageWhitelisted(const std::string& original_language,
      std::string* target_language);

  // Retrieves the dictionary mapping the number of times translation has been
  // denied for a language, creating it if necessary.
  DictionaryValue* GetTranslationDeniedCountDictionary();

  // Retrieves the dictionary mapping the number of times translation has been
  // accepted for a language, creating it if necessary.
  DictionaryValue* GetTranslationAcceptedCountDictionary();

  PrefService* prefs_;  // Weak.
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_PREFS_H_
