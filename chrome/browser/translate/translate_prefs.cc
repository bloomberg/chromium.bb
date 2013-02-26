// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_prefs.h"

#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"

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

// TranslatePrefs: public: -----------------------------------------------------

TranslatePrefs::TranslatePrefs(PrefService* user_prefs)
    : prefs_(user_prefs) {
}

bool TranslatePrefs::IsLanguageBlacklisted(
    const std::string& original_language) const {
  return IsValueBlacklisted(kPrefTranslateLanguageBlacklist, original_language);
}

void TranslatePrefs::BlacklistLanguage(const std::string& original_language) {
  BlacklistValue(kPrefTranslateLanguageBlacklist, original_language);
}

void TranslatePrefs::RemoveLanguageFromBlacklist(
    const std::string& original_language) {
  RemoveValueFromBlacklist(kPrefTranslateLanguageBlacklist, original_language);
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
  return !IsListEmpty(kPrefTranslateLanguageBlacklist);
}

void TranslatePrefs::ClearBlacklistedLanguages() {
  prefs_->ClearPref(kPrefTranslateLanguageBlacklist);
}

bool TranslatePrefs::HasBlacklistedSites() {
  return !IsListEmpty(kPrefTranslateSiteBlacklist);
}

void TranslatePrefs::ClearBlacklistedSites() {
  prefs_->ClearPref(kPrefTranslateSiteBlacklist);
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

bool TranslatePrefs::CanTranslate(PrefService* user_prefs,
    const std::string& original_language, const GURL& url) {
  TranslatePrefs prefs(user_prefs);
  if (prefs.IsSiteBlacklisted(url.HostNoBrackets()))
    return false;
  return (!prefs.IsLanguageBlacklisted(original_language));
}

bool TranslatePrefs::ShouldAutoTranslate(PrefService* user_prefs,
    const std::string& original_language, std::string* target_language) {
  TranslatePrefs prefs(user_prefs);
  return prefs.IsLanguageWhitelisted(original_language, target_language);
}

void TranslatePrefs::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kPrefTranslateLanguageBlacklist,
                             PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(kPrefTranslateSiteBlacklist,
                             PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(kPrefTranslateWhitelists,
                                   PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(kPrefTranslateDeniedCount,
                                   PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(kPrefTranslateAcceptedCount,
                                   PrefRegistrySyncable::SYNCABLE_PREF);
}

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
  if (!dict || dict->empty())
    return;
  for (DictionaryValue::key_iterator iter(dict->begin_keys());
       iter != dict->end_keys(); ++iter) {
    ListValue* list = NULL;
    if (!dict->GetList(*iter, &list) || !list)
      break;  // Dictionary has either been migrated or new format.
    std::string target_lang;
    if (list->empty() || !list->GetString(list->GetSize() - 1, &target_lang) ||
        target_lang.empty())
      dict->Remove(*iter, NULL);
     else
      dict->SetString(*iter, target_lang);
  }
}

// TranslatePrefs: private: ----------------------------------------------------

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
