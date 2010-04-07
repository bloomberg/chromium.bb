// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_prefs.h"

#include "base/string_util.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/scoped_pref_update.h"

const wchar_t TranslatePrefs::kPrefTranslateLanguageBlacklist[] =
    L"translate_language_blacklist";
const wchar_t TranslatePrefs::kPrefTranslateSiteBlacklist[] =
    L"translate_site_blacklist";
const wchar_t TranslatePrefs::kPrefTranslateWhitelists[] =
    L"translate_whitelists";

// TranslatePrefs: public: -----------------------------------------------------

TranslatePrefs::TranslatePrefs(PrefService* user_prefs)
    : prefs_(user_prefs) {
}

bool TranslatePrefs::IsLanguageBlacklisted(
    const std::string& original_language) {
  return IsValueBlacklisted(kPrefTranslateLanguageBlacklist, original_language);
}

void TranslatePrefs::BlacklistLanguage(const std::string& original_language) {
  ScopedPrefUpdate update(prefs_, kPrefTranslateLanguageBlacklist);
  BlacklistValue(kPrefTranslateLanguageBlacklist, original_language);
}

void TranslatePrefs::RemoveLanguageFromBlacklist(
    const std::string& original_language) {
  ScopedPrefUpdate update(prefs_, kPrefTranslateLanguageBlacklist);
  RemoveValueFromBlacklist(kPrefTranslateLanguageBlacklist, original_language);
}

bool TranslatePrefs::IsSiteBlacklisted(const std::string& site) {
  return IsValueBlacklisted(kPrefTranslateSiteBlacklist, site);
}

void TranslatePrefs::BlacklistSite(const std::string& site) {
  ScopedPrefUpdate update(prefs_, kPrefTranslateSiteBlacklist);
  BlacklistValue(kPrefTranslateSiteBlacklist, site);
}

void TranslatePrefs::RemoveSiteFromBlacklist(const std::string& site) {
  ScopedPrefUpdate update(prefs_, kPrefTranslateSiteBlacklist);
  RemoveValueFromBlacklist(kPrefTranslateSiteBlacklist, site);
}

bool TranslatePrefs::IsLanguagePairWhitelisted(
    const std::string& original_language,
    const std::string& target_language) {
  const DictionaryValue* dict = prefs_->GetDictionary(kPrefTranslateWhitelists);
  if (dict && !dict->empty()) {
    ListValue* whitelist = NULL;
    if (dict->GetList(ASCIIToWide(original_language), &whitelist) &&
        whitelist && !whitelist->empty() &&
        IsValueInList(whitelist, target_language))
      return true;
  }
  return false;
}

void TranslatePrefs::WhitelistLanguagePair(
    const std::string& original_language,
    const std::string& target_language) {
  DictionaryValue* dict = prefs_->GetMutableDictionary(
      kPrefTranslateWhitelists);
  if (!dict) {
    NOTREACHED() << "Unregistered translate whitelist pref";
    return;
  }
  ScopedPrefUpdate update(prefs_, kPrefTranslateWhitelists);
  std::wstring wide_original(ASCIIToWide(original_language));
  StringValue* language = new StringValue(target_language);
  ListValue* whitelist = NULL;
  if (dict->GetList(wide_original, &whitelist) && whitelist) {
    whitelist->Append(language);
  } else {
    ListValue new_whitelist;
    new_whitelist.Append(language);
    dict->Set(wide_original, new_whitelist.DeepCopy());
  }
  prefs_->ScheduleSavePersistentPrefs();
}

void TranslatePrefs::RemoveLanguagePairFromWhitelist(
    const std::string& original_language,
    const std::string& target_language) {
  DictionaryValue* dict = prefs_->GetMutableDictionary(
      kPrefTranslateWhitelists);
  if (!dict) {
    NOTREACHED() << "Unregistered translate whitelist pref";
    return;
  }
  ScopedPrefUpdate update(prefs_, kPrefTranslateWhitelists);
  ListValue* whitelist = NULL;
  std::wstring wide_original(ASCIIToWide(original_language));
  if (dict->GetList(wide_original, &whitelist) && whitelist) {
    StringValue language(target_language);
    if (whitelist->Remove(language) != -1) {
      if (whitelist->empty())  // If list is empty, remove key from dict.
        dict->Remove(wide_original, NULL);
      prefs_->ScheduleSavePersistentPrefs();
    }
  }
}

// TranslatePrefs: static: -----------------------------------------------------

bool TranslatePrefs::CanTranslate(PrefService* user_prefs,
    const std::string& original_language, const GURL& url) {
  TranslatePrefs prefs(user_prefs);
  if (prefs.IsSiteBlacklisted(url.HostNoBrackets()))
    return false;
  return (!prefs.IsLanguageBlacklisted(original_language));
}

bool TranslatePrefs::ShouldAutoTranslate(PrefService* user_prefs,
    const std::string& original_language,
    const std::string& target_language) {
  TranslatePrefs prefs(user_prefs);
  return prefs.IsLanguagePairWhitelisted(original_language, target_language);
}

void TranslatePrefs::RegisterUserPrefs(PrefService* user_prefs) {
  if (!user_prefs->FindPreference(kPrefTranslateLanguageBlacklist))
    user_prefs->RegisterListPref(kPrefTranslateLanguageBlacklist);
  if (!user_prefs->FindPreference(kPrefTranslateSiteBlacklist))
    user_prefs->RegisterListPref(kPrefTranslateSiteBlacklist);
  if (!user_prefs->FindPreference(kPrefTranslateWhitelists))
    user_prefs->RegisterDictionaryPref(kPrefTranslateWhitelists);
}

// TranslatePrefs: private: ----------------------------------------------------

bool TranslatePrefs::IsValueInList(const ListValue* list,
    const std::string& in_value) {
  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string value;
    if (list->GetString(i, &value) && value == in_value)
      return true;
  }
  return false;
}

bool TranslatePrefs::IsValueBlacklisted(const wchar_t* pref_id,
    const std::string& value) {
  const ListValue* blacklist = prefs_->GetList(pref_id);
  return (blacklist && !blacklist->empty() && IsValueInList(blacklist, value));
}

void TranslatePrefs::BlacklistValue(const wchar_t* pref_id,
    const std::string& value) {
  ListValue* blacklist = prefs_->GetMutableList(pref_id);
  if (!blacklist) {
    NOTREACHED() << "Unregistered translate blacklist pref";
    return;
  }
  blacklist->Append(new StringValue(value));
  prefs_->ScheduleSavePersistentPrefs();
}

void TranslatePrefs::RemoveValueFromBlacklist(const wchar_t* pref_id,
    const std::string& value) {
  ListValue* blacklist = prefs_->GetMutableList(pref_id);
  if (!blacklist) {
    NOTREACHED() << "Unregistered translate blacklist pref";
    return;
  }
  StringValue string_value(value);
  if (blacklist->Remove(string_value) != -1)
    prefs_->ScheduleSavePersistentPrefs();
}
