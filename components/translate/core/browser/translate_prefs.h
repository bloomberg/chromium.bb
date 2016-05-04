// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_PREFS_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_PREFS_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/gurl.h"

class PrefService;
class Profile;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace translate {

// Feature flag for "Translate UI 2016 Q2" project.
extern const base::Feature kTranslateUI2016Q2;

class TranslateAcceptLanguages;

// Allows updating denial times for a specific language while maintaining the
// maximum list size and ensuring PrefObservers are notified of change values.
class DenialTimeUpdate {
 public:
  DenialTimeUpdate(PrefService* prefs,
                   const std::string& language,
                   size_t max_denial_count);
  ~DenialTimeUpdate();

  // Gets the list of timestamps when translation was denied. Guaranteed to
  // be non-null, potentially inserts a new listvalue into the dictionary if
  // necessary.
  base::ListValue* GetDenialTimes();

  // Gets the oldest denial time on record. Will return base::Time::max() if
  // no denial time has been recorded yet.
  base::Time GetOldestDenialTime();

  // Records a new denial time. Does not ensure ordering of denial times - it is
  // up to the user to ensure times are in monotonic order.
  void AddDenialTime(base::Time denial_time);

 private:
  DictionaryPrefUpdate denial_time_dict_update_;
  std::string language_;
  size_t max_denial_count_;
  base::ListValue* time_list_;  // Weak, owned by the containing prefs service.
};

// The wrapper of PrefService object for Translate.
//
// It is assumed that |prefs_| is alive while this instance is alive.
class TranslatePrefs {
 public:
  static const char kPrefTranslateSiteBlacklist[];
  static const char kPrefTranslateWhitelists[];
  static const char kPrefTranslateDeniedCount[];
  static const char kPrefTranslateIgnoredCount[];
  static const char kPrefTranslateAcceptedCount[];
  static const char kPrefTranslateBlockedLanguages[];
  static const char kPrefTranslateLastDeniedTimeForLanguage[];
  static const char kPrefTranslateTooOftenDeniedForLanguage[];

  // |preferred_languages_pref| is only used on Chrome OS, other platforms must
  // pass NULL.
  TranslatePrefs(PrefService* user_prefs,
                 const char* accept_languages_pref,
                 const char* preferred_languages_pref);

  // Sets the country that the application is run in. Determined by the
  // VariationsService, can be left empty. Used by TranslateExperiment.
  void SetCountry(const std::string& country);
  std::string GetCountry() const;

  // Resets the blocked languages list, the sites blacklist, the languages
  // whitelist, and the accepted/denied counts.
  void ResetToDefaults();

  bool IsBlockedLanguage(const std::string& original_language) const;
  void BlockLanguage(const std::string& original_language);
  void UnblockLanguage(const std::string& original_language);

  // Removes a language from the old blacklist. Only used internally for
  // diagnostics. Don't use this if there is no special reason.
  void RemoveLanguageFromLegacyBlacklist(const std::string& original_language);

  bool IsSiteBlacklisted(const std::string& site) const;
  void BlacklistSite(const std::string& site);
  void RemoveSiteFromBlacklist(const std::string& site);

  bool HasWhitelistedLanguagePairs() const;

  bool IsLanguagePairWhitelisted(const std::string& original_language,
                                 const std::string& target_language);
  void WhitelistLanguagePair(const std::string& original_language,
                             const std::string& target_language);
  void RemoveLanguagePairFromWhitelist(const std::string& original_language,
                                       const std::string& target_language);

  // Will return true if at least one language has been blacklisted.
  bool HasBlockedLanguages() const;

  // Will return true if at least one site has been blacklisted.
  bool HasBlacklistedSites() const;

  // These methods are used to track how many times the user has denied the
  // translation for a specific language. (So we can present a UI to black-list
  // that language if the user keeps denying translations).
  int GetTranslationDeniedCount(const std::string& language) const;
  void IncrementTranslationDeniedCount(const std::string& language);
  void ResetTranslationDeniedCount(const std::string& language);

  // These methods are used to track how many times the user has ignored the
  // translation bubble for a specific language.
  int GetTranslationIgnoredCount(const std::string& language) const;
  void IncrementTranslationIgnoredCount(const std::string& language);
  void ResetTranslationIgnoredCount(const std::string& language);

  // These methods are used to track how many times the user has accepted the
  // translation for a specific language. (So we can present a UI to white-list
  // that language if the user keeps accepting translations).
  int GetTranslationAcceptedCount(const std::string& language);
  void IncrementTranslationAcceptedCount(const std::string& language);
  void ResetTranslationAcceptedCount(const std::string& language);

  // Update the last time on closing the Translate UI without translation.
  void UpdateLastDeniedTime(const std::string& language);

  // Returns true if translation is denied too often.
  bool IsTooOftenDenied(const std::string& language) const;

  // Resets the prefs of denial state. Only used internally for diagnostics.
  void ResetDenialState();

  // Gets the language list of the language settings.
  void GetLanguageList(std::vector<std::string>* languages) const;

  // Updates the language list of the language settings.
  void UpdateLanguageList(const std::vector<std::string>& languages);

  bool CanTranslateLanguage(TranslateAcceptLanguages* accept_languages,
                            const std::string& language);
  bool ShouldAutoTranslate(const std::string& original_language,
                           std::string* target_language);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  static void MigrateUserPrefs(PrefService* user_prefs,
                               const char* accept_languages_pref);

 private:
  friend class TranslatePrefsTest;
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest, CreateBlockedLanguages);
  FRIEND_TEST_ALL_PREFIXES(TranslatePrefsTest,
                           CreateBlockedLanguagesNonEnglishUI);

  // Merges two language sets to migrate to the language setting UI.
  static void CreateBlockedLanguages(
      std::vector<std::string>* blocked_languages,
      const std::vector<std::string>& blacklisted_languages,
      const std::vector<std::string>& accept_languages);

  void ClearBlockedLanguages();
  void ClearBlacklistedSites();
  void ClearWhitelistedLanguagePairs();
  bool IsValueBlacklisted(const char* pref_id, const std::string& value) const;
  void BlacklistValue(const char* pref_id, const std::string& value);
  void RemoveValueFromBlacklist(const char* pref_id, const std::string& value);
  bool IsValueInList(const base::ListValue* list,
                     const std::string& value) const;
  bool IsListEmpty(const char* pref_id) const;
  bool IsDictionaryEmpty(const char* pref_id) const;

  // Path to the preference storing the accept languages.
  const std::string accept_languages_pref_;
#if defined(OS_CHROMEOS)
  // Path to the preference storing the preferred languages.
  // Only used on ChromeOS.
  std::string preferred_languages_pref_;
#endif

  // Retrieves the dictionary mapping the number of times translation has been
  // denied for a language, creating it if necessary.
  base::DictionaryValue* GetTranslationDeniedCountDictionary();

  // Retrieves the dictionary mapping the number of times translation has been
  // accepted for a language, creating it if necessary.
  base::DictionaryValue* GetTranslationAcceptedCountDictionary() const;

  PrefService* prefs_;  // Weak.

  std::string country_;  // The country the app runs in.

  DISALLOW_COPY_AND_ASSIGN(TranslatePrefs);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_PREFS_H_
