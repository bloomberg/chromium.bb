// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellcheck_host.h"

#include "base/string_split.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellcheck_host_impl.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_common.h"

// static
scoped_refptr<SpellCheckHost> SpellCheckHost::Create(
    SpellCheckHostObserver* observer,
    const std::string& language,
    net::URLRequestContextGetter* request_context_getter) {
  scoped_refptr<SpellCheckHostImpl> host =
      new SpellCheckHostImpl(observer,
                             language,
                             request_context_getter);
  if (!host)
    return NULL;

  host->Initialize();
  return host;
}

// static
int SpellCheckHost::GetSpellCheckLanguages(
    Profile* profile,
    std::vector<std::string>* languages) {
  StringPrefMember accept_languages_pref;
  StringPrefMember dictionary_language_pref;
  accept_languages_pref.Init(prefs::kAcceptLanguages, profile->GetPrefs(),
                             NULL);
  dictionary_language_pref.Init(prefs::kSpellCheckDictionary,
                                profile->GetPrefs(), NULL);
  std::string dictionary_language = dictionary_language_pref.GetValue();

  // The current dictionary language should be there.
  languages->push_back(dictionary_language);

  // Now scan through the list of accept languages, and find possible mappings
  // from this list to the existing list of spell check languages.
  std::vector<std::string> accept_languages;

  if (SpellCheckerPlatform::SpellCheckerAvailable())
    SpellCheckerPlatform::GetAvailableLanguages(&accept_languages);
  else
    base::SplitString(accept_languages_pref.GetValue(), ',', &accept_languages);

  for (std::vector<std::string>::const_iterator i = accept_languages.begin();
       i != accept_languages.end(); ++i) {
    std::string language =
        SpellCheckCommon::GetCorrespondingSpellCheckLanguage(*i);
    if (!language.empty() &&
        std::find(languages->begin(), languages->end(), language) ==
        languages->end()) {
      languages->push_back(language);
    }
  }

  for (size_t i = 0; i < languages->size(); ++i) {
    if ((*languages)[i] == dictionary_language)
      return i;
  }
  return -1;
}
