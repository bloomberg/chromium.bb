// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_accept_languages.h"

#include "base/bind.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/translate/translate_util.h"
#include "content/public/browser/notification_source.h"
#include "net/url_request/url_fetcher.h"
#include "ui/base/l10n/l10n_util.h"

TranslateAcceptLanguages::TranslateAcceptLanguages() {
}

TranslateAcceptLanguages::~TranslateAcceptLanguages() {
}

// static
bool TranslateAcceptLanguages::CanBeAcceptLanguage(
    const std::string& language) {
  std::string accept_language = language;
  TranslateUtil::ToChromeLanguageSynonym(&accept_language);

  const std::string locale = g_browser_process->GetApplicationLocale();
  std::vector<std::string> accept_language_codes;
  l10n_util::GetAcceptLanguagesForLocale(locale, &accept_language_codes);

  if (std::find(accept_language_codes.begin(),
                accept_language_codes.end(),
                accept_language) != accept_language_codes.end()) {
    return true;
  }

  return false;
}

bool TranslateAcceptLanguages::IsAcceptLanguage(Profile* profile,
                                                const std::string& language) {
  DCHECK(profile);

  PrefService* pref_service = profile->GetPrefs();
  PrefServiceLanguagesMap::const_iterator iter =
      accept_languages_.find(pref_service);
  if (iter == accept_languages_.end()) {
    InitAcceptLanguages(pref_service);
    // Listen for this profile going away, in which case we would need to clear
    // the accepted languages for the profile.
    notification_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                                content::Source<Profile>(profile));
    // Also start listening for changes in the accept languages.
    DCHECK(pref_change_registrars_.find(pref_service) ==
           pref_change_registrars_.end());
    PrefChangeRegistrar* pref_change_registrar = new PrefChangeRegistrar;
    pref_change_registrar->Init(pref_service);
    pref_change_registrar->Add(
        prefs::kAcceptLanguages,
        base::Bind(&TranslateAcceptLanguages::InitAcceptLanguages,
                   base::Unretained(this),
                   pref_service));
    pref_change_registrars_[pref_service] = pref_change_registrar;

    iter = accept_languages_.find(pref_service);
  }

  std::string accept_language = language;
  TranslateUtil::ToChromeLanguageSynonym(&accept_language);
  return iter->second.count(accept_language) != 0;
}

void TranslateAcceptLanguages::InitAcceptLanguages(PrefService* prefs) {
  DCHECK(prefs);

  // We have been asked for this profile, build the languages.
  std::string accept_langs_str = prefs->GetString(prefs::kAcceptLanguages);
  std::vector<std::string> accept_langs_list;
  LanguageSet accept_langs_set;
  base::SplitString(accept_langs_str, ',', &accept_langs_list);
  std::vector<std::string>::const_iterator iter;

  for (iter = accept_langs_list.begin();
       iter != accept_langs_list.end(); ++iter) {
    // Get rid of the locale extension if any (ex: en-US -> en), but for Chinese
    // for which the CLD reports zh-CN and zh-TW.
    std::string accept_lang(*iter);
    size_t index = iter->find("-");
    if (index != std::string::npos && *iter != "zh-CN" && *iter != "zh-TW")
      accept_lang = iter->substr(0, index);

    accept_langs_set.insert(accept_lang);
  }
  accept_languages_[prefs] = accept_langs_set;
}

void TranslateAcceptLanguages::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);

  PrefService* pref_service =
      content::Source<Profile>(source).ptr()->GetPrefs();
  notification_registrar_.Remove(this,
                                 chrome::NOTIFICATION_PROFILE_DESTROYED,
                                 source);
  size_t count = accept_languages_.erase(pref_service);
  // We should know about this profile since we are listening for
  // notifications on it.
  DCHECK_EQ(1u, count);
  PrefChangeRegistrar* pref_change_registrar =
      pref_change_registrars_[pref_service];
  count = pref_change_registrars_.erase(pref_service);
  DCHECK_EQ(1u, count);
  delete pref_change_registrar;
}
