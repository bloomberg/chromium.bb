// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_prefs.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_accept_languages.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/translate/translate_util.h"
#include "components/user_prefs/pref_registry_syncable.h"

const char TranslatePrefs::kPrefTranslateLanguageBlacklist[] =
    "translate_language_blacklist";
const char TranslatePrefs::kPrefTranslateSiteBlacklist[] =
    "translate_site_blacklist";
const char TranslatePrefs::kPrefTranslateWhitelists[] =
    "translate_whitelists";
const char TranslatePrefs::kPrefTranslateDeniedCount[] =
    "translate_denied_count";
const char TranslatePrefs::kPrefTranslateAcceptedCount[] =
    "translate_accepted_count";
const char TranslatePrefs::kPrefTranslateBlockedLanguages[] =
    "translate_blocked_languages";

namespace {

void GetBlacklistedLanguages(const PrefService* prefs,
                             std::vector<std::string>* languages) {
  DCHECK(languages->empty());

  const char* key = TranslatePrefs::kPrefTranslateLanguageBlacklist;
  const ListValue* list = prefs->GetList(key);
  for (ListValue::const_iterator it = list->begin(); it != list->end(); ++it) {
    std::string lang;
    (*it)->GetAsString(&lang);
    languages->push_back(lang);
  }
}

}  // namespace

namespace {

void AppendLanguageToAcceptLanguages(PrefService* prefs,
                                     const std::string& language) {
  if (!TranslateAcceptLanguages::CanBeAcceptLanguage(language))
    return;

  std::string accept_language = language;
  TranslateUtil::ToChromeLanguageSynonym(&accept_language);

  std::string accept_languages_str = prefs->GetString(prefs::kAcceptLanguages);
  std::vector<std::string> accept_languages;
  base::SplitString(accept_languages_str, ',', &accept_languages);
  if (std::find(accept_languages.begin(),
                accept_languages.end(),
                accept_language) == accept_languages.end()) {
    accept_languages.push_back(accept_language);
  }
  accept_languages_str = JoinString(accept_languages, ',');
  prefs->SetString(prefs::kAcceptLanguages, accept_languages_str);
}

}  // namespace

// TranslatePrefs: public: -----------------------------------------------------

TranslatePrefs::TranslatePrefs(PrefService* user_prefs)
    : prefs_(user_prefs) {
}

bool TranslatePrefs::IsBlockedLanguage(
    const std::string& original_language) const {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableTranslateSettings)) {
    return IsValueBlacklisted(kPrefTranslateBlockedLanguages,
                              original_language);
  } else {
    return IsValueBlacklisted(kPrefTranslateLanguageBlacklist,
                              original_language);
  }
}

void TranslatePrefs::BlockLanguage(
    const std::string& original_language) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableTranslateSettings)) {
    BlacklistValue(kPrefTranslateBlockedLanguages, original_language);
    AppendLanguageToAcceptLanguages(prefs_, original_language);
  } else {
    BlacklistValue(kPrefTranslateLanguageBlacklist, original_language);
  }
}

void TranslatePrefs::UnblockLanguage(
    const std::string& original_language) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableTranslateSettings)) {
    RemoveValueFromBlacklist(kPrefTranslateBlockedLanguages,
                             original_language);
  } else {
    RemoveValueFromBlacklist(kPrefTranslateLanguageBlacklist,
                             original_language);
  }
}

void TranslatePrefs::RemoveLanguageFromLegacyBlacklist(
    const std::string& original_language) {
  RemoveValueFromBlacklist(kPrefTranslateLanguageBlacklist,
                           original_language);
}

bool TranslatePrefs::IsSiteBlacklisted(const std::string& site) const {
  return IsValueBlacklisted(kPrefTranslateSiteBlacklist, site);
}

void TranslatePrefs::BlacklistSite(const std::string& site) {
  BlacklistValue(kPrefTranslateSiteBlacklist, site);
}

void TranslatePrefs::RemoveSiteFromBlacklist(const std::string& site) {
  RemoveValueFromBlacklist(kPrefTranslateSiteBlacklist, site);
}

bool TranslatePrefs::IsLanguagePairWhitelisted(
    const std::string& original_language,
    const std::string& target_language) {
  const DictionaryValue* dict = prefs_->GetDictionary(kPrefTranslateWhitelists);
  if (dict && !dict->empty()) {
    std::string auto_target_lang;
    if (dict->GetString(original_language, &auto_target_lang) &&
        auto_target_lang == target_language)
      return true;
  }
  return false;
}

void TranslatePrefs::WhitelistLanguagePair(
    const std::string& original_language,
    const std::string& target_language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateWhitelists);
  DictionaryValue* dict = update.Get();
  if (!dict) {
    NOTREACHED() << "Unregistered translate whitelist pref";
    return;
  }
  dict->SetString(original_language, target_language);
}

void TranslatePrefs::RemoveLanguagePairFromWhitelist(
    const std::string& original_language,
    const std::string& target_language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateWhitelists);
  DictionaryValue* dict = update.Get();
  if (!dict) {
    NOTREACHED() << "Unregistered translate whitelist pref";
    return;
  }
  dict->Remove(original_language, NULL);
}

bool TranslatePrefs::HasBlacklistedLanguages() const {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableTranslateSettings))
    return !IsListEmpty(kPrefTranslateBlockedLanguages);
  else
    return !IsListEmpty(kPrefTranslateLanguageBlacklist);
}

void TranslatePrefs::ClearBlacklistedLanguages() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableTranslateSettings))
    prefs_->ClearPref(kPrefTranslateBlockedLanguages);
  else
    prefs_->ClearPref(kPrefTranslateLanguageBlacklist);
}

bool TranslatePrefs::HasBlacklistedSites() const {
  return !IsListEmpty(kPrefTranslateSiteBlacklist);
}

void TranslatePrefs::ClearBlacklistedSites() {
  prefs_->ClearPref(kPrefTranslateSiteBlacklist);
}

bool TranslatePrefs::HasWhitelistedLanguagePairs() const {
  return !IsDictionaryEmpty(kPrefTranslateWhitelists);
}

void TranslatePrefs::ClearWhitelistedLanguagePairs() {
  prefs_->ClearPref(kPrefTranslateWhitelists);
}

int TranslatePrefs::GetTranslationDeniedCount(
    const std::string& language) const {
  const DictionaryValue* dict =
      prefs_->GetDictionary(kPrefTranslateDeniedCount);
  int count = 0;
  return dict->GetInteger(language, &count) ? count : 0;
}

void TranslatePrefs::IncrementTranslationDeniedCount(
    const std::string& language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateDeniedCount);
  DictionaryValue* dict = update.Get();

  int count = 0;
  dict->GetInteger(language, &count);
  dict->SetInteger(language, count + 1);
}

void TranslatePrefs::ResetTranslationDeniedCount(const std::string& language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateDeniedCount);
  update.Get()->SetInteger(language, 0);
}

int TranslatePrefs::GetTranslationAcceptedCount(const std::string& language) {
  const DictionaryValue* dict =
      prefs_->GetDictionary(kPrefTranslateAcceptedCount);
  int count = 0;
  return dict->GetInteger(language, &count) ? count : 0;
}

void TranslatePrefs::IncrementTranslationAcceptedCount(
    const std::string& language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateAcceptedCount);
  DictionaryValue* dict = update.Get();
  int count = 0;
  dict->GetInteger(language, &count);
  dict->SetInteger(language, count + 1);
}

void TranslatePrefs::ResetTranslationAcceptedCount(
    const std::string& language) {
  DictionaryPrefUpdate update(prefs_, kPrefTranslateAcceptedCount);
  update.Get()->SetInteger(language, 0);
}

// TranslatePrefs: public, static: ---------------------------------------------

// static
bool TranslatePrefs::CanTranslateLanguage(Profile* profile,
                                          const std::string& language) {
  TranslatePrefs translate_prefs(profile->GetPrefs());
  bool blocked = translate_prefs.IsBlockedLanguage(language);

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableTranslateSettings)) {
    bool is_accept_language =
        TranslateManager::IsAcceptLanguage(profile, language);
    bool can_be_accept_language =
        TranslateAcceptLanguages::CanBeAcceptLanguage(language);

    // Don't translate any user black-listed languages. Checking
    // |is_accept_language| is necessary because if the user eliminates the
    // language from the preference, it is natural to forget whether or not
    // the language should be translated. Checking |cannot_be_accept_language|
    // is also necessary because some minor languages can't be selected in the
    // language preference even though the language is available in Translate
    // server.
    if (blocked && (is_accept_language || !can_be_accept_language))
      return false;
  } else {
    // Don't translate any user user selected language.
    if (blocked)
      return false;
  }

  return true;
}

// static
bool TranslatePrefs::ShouldAutoTranslate(PrefService* user_prefs,
    const std::string& original_language, std::string* target_language) {
  TranslatePrefs prefs(user_prefs);
  return prefs.IsLanguageWhitelisted(original_language, target_language);
}

// static
void TranslatePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kPrefTranslateLanguageBlacklist,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(kPrefTranslateSiteBlacklist,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateWhitelists,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateDeniedCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kPrefTranslateAcceptedCount,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(kPrefTranslateBlockedLanguages,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

// static
void TranslatePrefs::MigrateUserPrefs(PrefService* user_prefs) {
  // Old format of kPrefTranslateWhitelists
  // - original language -> list of target langs to auto-translate
  // - list of langs is in order of being enabled i.e. last in list is the
  //   most recent language that user enabled via
  //   Always translate |source_lang| to |target_lang|"
  // - this results in a one-to-n relationship between source lang and target
  //   langs.
  // New format:
  // - original language -> one target language to auto-translate
  // - each time that the user enables the "Always translate..." option, that
  //   target lang overwrites the previous one.
  // - this results in a one-to-one relationship between source lang and target
  //   lang
  // - we replace old list of target langs with the last target lang in list,
  //   assuming the last (i.e. most recent) target lang is what user wants to
  //   keep auto-translated.
  DictionaryPrefUpdate update(user_prefs, kPrefTranslateWhitelists);
  DictionaryValue* dict = update.Get();
  if (dict && !dict->empty()) {
    DictionaryValue::Iterator iter(*dict);
    while (!iter.IsAtEnd()) {
      const ListValue* list = NULL;
      if (!iter.value().GetAsList(&list) || !list)
        break;  // Dictionary has either been migrated or new format.
      std::string key = iter.key();
      // Advance the iterator before removing the current element.
      iter.Advance();
      std::string target_lang;
      if (list->empty() ||
          !list->GetString(list->GetSize() - 1, &target_lang) ||
          target_lang.empty()) {
        dict->Remove(key, NULL);
      } else {
        dict->SetString(key, target_lang);
      }
    }
  }

  // Get the union of the blacklist and the Accept languages, and set this to
  // the new language set 'translate_blocked_languages'. This is used for the
  // settings UI for Translate and configration to determine which langauage
  // should be translated instead of the blacklist. The blacklist is no longer
  // used after launching the settings UI.
  // After that, Set 'translate_languages_not_translate' to Accept languages to
  // enable settings for users.
  bool merged = user_prefs->HasPrefPath(kPrefTranslateBlockedLanguages);

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool enabled_translate_settings =
      command_line.HasSwitch(switches::kEnableTranslateSettings);

  if (!merged && enabled_translate_settings) {
    std::vector<std::string> blacklisted_languages;
    GetBlacklistedLanguages(user_prefs, &blacklisted_languages);

    std::string accept_languages_str =
        user_prefs->GetString(prefs::kAcceptLanguages);
    std::vector<std::string> accept_languages;
    base::SplitString(accept_languages_str, ',', &accept_languages);

    std::vector<std::string> blocked_languages;
    CreateBlockedLanguages(&blocked_languages,
                           blacklisted_languages,
                           accept_languages);

    // Create the new preference kPrefTranslateBlockedLanguages.
    {
      ListValue* blocked_languages_list = new ListValue();
      for (std::vector<std::string>::const_iterator it =
               blocked_languages.begin();
           it != blocked_languages.end(); ++it) {
        blocked_languages_list->Append(new StringValue(*it));
      }
      ListPrefUpdate update(user_prefs, kPrefTranslateBlockedLanguages);
      ListValue* list = update.Get();
      DCHECK(list != NULL);
      list->Swap(blocked_languages_list);
    }

    // Update kAcceptLanguages
    for (std::vector<std::string>::const_iterator it =
             blocked_languages.begin();
         it != blocked_languages.end(); ++it) {
      std::string lang = *it;
      TranslateUtil::ToChromeLanguageSynonym(&lang);
      bool not_found =
          std::find(accept_languages.begin(), accept_languages.end(), lang) ==
          accept_languages.end();
      if (not_found)
        accept_languages.push_back(lang);
    }

    std::string new_accept_languages_str = JoinString(accept_languages, ",");
    user_prefs->SetString(prefs::kAcceptLanguages, new_accept_languages_str);
  }
}

// TranslatePrefs: private: ----------------------------------------------------

// static
void TranslatePrefs::CreateBlockedLanguages(
    std::vector<std::string>* blocked_languages,
    const std::vector<std::string>& blacklisted_languages,
    const std::vector<std::string>& accept_languages) {
  DCHECK(blocked_languages->empty());

  std::set<std::string> result;

  for (std::vector<std::string>::const_iterator it =
           blacklisted_languages.begin();
       it != blacklisted_languages.end(); ++it) {
    result.insert(*it);
  }

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  std::string ui_lang = TranslateManager::GetLanguageCode(app_locale);
  bool is_ui_english = ui_lang == "en" ||
      StartsWithASCII(ui_lang, "en-", false);

  for (std::vector<std::string>::const_iterator it = accept_languages.begin();
       it != accept_languages.end(); ++it) {
    std::string lang = *it;
    TranslateUtil::ToTranslateLanguageSynonym(&lang);

    // Regarding http://crbug.com/36182, even though English exists in Accept
    // language list, English could be translated on non-English locale.
    if (lang == "en" && !is_ui_english)
      continue;

    result.insert(lang);
  }

  blocked_languages->insert(blocked_languages->begin(),
                            result.begin(), result.end());
}

bool TranslatePrefs::IsValueInList(const ListValue* list,
    const std::string& in_value) const {
  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string value;
    if (list->GetString(i, &value) && value == in_value)
      return true;
  }
  return false;
}

bool TranslatePrefs::IsValueBlacklisted(const char* pref_id,
    const std::string& value) const {
  const ListValue* blacklist = prefs_->GetList(pref_id);
  return (blacklist && !blacklist->empty() && IsValueInList(blacklist, value));
}

void TranslatePrefs::BlacklistValue(const char* pref_id,
    const std::string& value) {
  {
    ListPrefUpdate update(prefs_, pref_id);
    ListValue* blacklist = update.Get();
    if (!blacklist) {
      NOTREACHED() << "Unregistered translate blacklist pref";
      return;
    }
    blacklist->Append(new StringValue(value));
  }
}

void TranslatePrefs::RemoveValueFromBlacklist(const char* pref_id,
    const std::string& value) {
  ListPrefUpdate update(prefs_, pref_id);
  ListValue* blacklist = update.Get();
  if (!blacklist) {
    NOTREACHED() << "Unregistered translate blacklist pref";
    return;
  }
  StringValue string_value(value);
  blacklist->Remove(string_value, NULL);
}

bool TranslatePrefs::IsLanguageWhitelisted(
    const std::string& original_language, std::string* target_language) const {
  const DictionaryValue* dict = prefs_->GetDictionary(kPrefTranslateWhitelists);
  if (dict && dict->GetString(original_language, target_language)) {
    DCHECK(!target_language->empty());
    return !target_language->empty();
  }
  return false;
}

bool TranslatePrefs::IsListEmpty(const char* pref_id) const {
  const ListValue* blacklist = prefs_->GetList(pref_id);
  return (blacklist == NULL || blacklist->empty());
}

bool TranslatePrefs::IsDictionaryEmpty(const char* pref_id) const {
  const DictionaryValue* dict = prefs_->GetDictionary(pref_id);
  return (dict == NULL || dict->empty());
}
