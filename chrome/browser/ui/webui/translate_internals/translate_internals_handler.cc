// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/translate_internals/translate_internals_handler.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

void TranslateInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("removePrefItem", base::Bind(
      &TranslateInternalsHandler::OnRemovePrefItem, base::Unretained(this)));
  web_ui()->RegisterMessageCallback("requestInfo", base::Bind(
      &TranslateInternalsHandler::OnRequestInfo, base::Unretained(this)));
}

void TranslateInternalsHandler::OnRemovePrefItem(const base::ListValue* args) {
  content::WebContents* web_contents = web_ui()->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  PrefService* prefs = profile->GetOriginalProfile()->GetPrefs();
  TranslatePrefs translate_prefs(prefs);

  std::string pref_name;
  if (!args->GetString(0, &pref_name))
    return;

  if (pref_name == "language_blacklist") {
    std::string language;
    if (!args->GetString(1, &language))
      return;
    translate_prefs.RemoveLanguageFromBlacklist(language);
  } else if (pref_name == "site_blacklist") {
    std::string site;
    if (!args->GetString(1, &site))
      return;
    translate_prefs.RemoveSiteFromBlacklist(site);
  } else if (pref_name == "whitelists") {
    std::string from, to;
    if (!args->GetString(1, &from))
      return;
    if (!args->GetString(2, &to))
      return;
    translate_prefs.RemoveLanguagePairFromWhitelist(from, to);
  } else {
    return;
  }

  base::ListValue unused;
  SendPrefsToJs();
}

void TranslateInternalsHandler::OnRequestInfo(const base::ListValue* /*args*/) {
  SendPrefsToJs();
}

void TranslateInternalsHandler::SendMessageToJs(const std::string& message,
                                                const base::Value& value) {
  const char func[] = "cr.translateInternals.messageHandler";
  base::StringValue message_data(message);
  web_ui()->CallJavascriptFunction(func, message_data, value);
}

void TranslateInternalsHandler::SendPrefsToJs() {
  content::WebContents* web_contents = web_ui()->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  PrefService* prefs = profile->GetOriginalProfile()->GetPrefs();

  base::DictionaryValue dict;

  static const char* keys[] = {
    prefs::kEnableTranslate,
    TranslatePrefs::kPrefTranslateLanguageBlacklist,
    TranslatePrefs::kPrefTranslateSiteBlacklist,
    TranslatePrefs::kPrefTranslateWhitelists,
    TranslatePrefs::kPrefTranslateDeniedCount,
    TranslatePrefs::kPrefTranslateAcceptedCount,
  };

  for (size_t i = 0; i < arraysize(keys); ++i) {
    const char* key = keys[i];
    const PrefService::Preference* pref = prefs->FindPreference(key);
    if (pref)
      dict.Set(key, pref->GetValue()->DeepCopy());
  }

  SendMessageToJs("prefsUpdated", dict);
}
