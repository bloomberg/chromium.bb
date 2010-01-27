// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_PREFS_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_PREFS_H_

#include <vector>

#include "googleurl/src/gurl.h"

class ListValue;
class PrefService;

class TranslatePrefs {
 public:
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

  static bool CanTranslate(PrefService* user_prefs,
      const std::string& original_language, const GURL& url);
  static bool ShouldAutoTranslate(PrefService* user_prefs,
      const std::string& original_language,
      const std::string& target_language);

 private:
  void Register();
  bool IsValueBlacklisted(const wchar_t* pref_id, const std::string& value);
  void BlacklistValue(const wchar_t* pref_id, const std::string& value);
  void RemoveValueFromBlacklist(const wchar_t* pref_id,
      const std::string& value);
  bool IsValueInList(const ListValue* list, const std::string& value);

  PrefService* prefs_;  // Weak.
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_PREFS_H_
