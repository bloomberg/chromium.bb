// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/translate_internals/translate_internals_handler.h"

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_error_details.h"
#include "chrome/browser/translate/translate_event_details.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/translate/language_detection_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

TranslateInternalsHandler::TranslateInternalsHandler() {
  TranslateManager::GetInstance()->AddObserver(this);
}

TranslateInternalsHandler::~TranslateInternalsHandler() {
  TranslateManager::GetInstance()->RemoveObserver(this);
}

void TranslateInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("removePrefItem", base::Bind(
      &TranslateInternalsHandler::OnRemovePrefItem, base::Unretained(this)));
  web_ui()->RegisterMessageCallback("requestInfo", base::Bind(
      &TranslateInternalsHandler::OnRequestInfo, base::Unretained(this)));
}

void TranslateInternalsHandler::OnLanguageDetection(
    const LanguageDetectionDetails& details) {
  if (!TranslateManager::IsTranslatableURL(details.url))
    return;

  base::DictionaryValue dict;
  dict.Set("time",
           new base::FundamentalValue(details.time.ToJsTime()));
  dict.Set("url",
           new base::StringValue(details.url.spec()));
  dict.Set("content_language",
           new base::StringValue(details.content_language));
  dict.Set("cld_language",
           new base::StringValue(details.cld_language));
  dict.Set("is_cld_reliable",
           new base::FundamentalValue(details.is_cld_reliable));
  dict.Set("html_root_language",
           new base::StringValue(details.html_root_language));
  dict.Set("adopted_language",
           new base::StringValue(details.adopted_language));
  dict.Set("content", new base::StringValue(details.contents));
  SendMessageToJs("languageDetectionInfoAdded", dict);
}

void TranslateInternalsHandler::OnTranslateError(
    const TranslateErrorDetails& details) {
  base::DictionaryValue dict;
  dict.Set("time",
           new base::FundamentalValue(details.time.ToJsTime()));
  dict.Set("url",
           new base::StringValue(details.url.spec()));
  dict.Set("error",
           new base::FundamentalValue(details.error));
  SendMessageToJs("translateErrorDetailsAdded", dict);
}

void TranslateInternalsHandler::OnTranslateEvent(
    const TranslateEventDetails& details) {
  base::DictionaryValue dict;
  dict.Set("time", new base::FundamentalValue(details.time.ToJsTime()));
  dict.Set("filename", new base::StringValue(details.filename));
  dict.Set("line", new base::FundamentalValue(details.line));
  dict.Set("message", new base::StringValue(details.message));
  SendMessageToJs("translateEventDetailsAdded", dict);
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

  if (pref_name == "blocked_languages") {
    std::string language;
    if (!args->GetString(1, &language))
      return;
    translate_prefs.UnblockLanguage(language);
  } else if (pref_name == "language_blacklist") {
    std::string language;
    if (!args->GetString(1, &language))
      return;
    translate_prefs.RemoveLanguageFromLegacyBlacklist(language);
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

  SendPrefsToJs();
}

void TranslateInternalsHandler::OnRequestInfo(const base::ListValue* /*args*/) {
  SendPrefsToJs();
  SendSupportedLanguagesToJs();
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

  std::vector<std::string> keys;
  keys.push_back(prefs::kEnableTranslate);

  keys.push_back(TranslatePrefs::kPrefTranslateBlockedLanguages);
  keys.push_back(TranslatePrefs::kPrefTranslateLanguageBlacklist);
  keys.push_back(TranslatePrefs::kPrefTranslateSiteBlacklist);
  keys.push_back(TranslatePrefs::kPrefTranslateWhitelists);
  keys.push_back(TranslatePrefs::kPrefTranslateDeniedCount);
  keys.push_back(TranslatePrefs::kPrefTranslateAcceptedCount);

  for (std::vector<std::string>::const_iterator it = keys.begin();
       it != keys.end(); ++it) {
    const std::string& key = *it;
    const PrefService::Preference* pref = prefs->FindPreference(key.c_str());
    if (pref)
      dict.Set(key, pref->GetValue()->DeepCopy());
  }

  SendMessageToJs("prefsUpdated", dict);
}

void TranslateInternalsHandler::SendSupportedLanguagesToJs() {
  base::DictionaryValue dict;

  std::vector<std::string> languages;
  TranslateManager::GetSupportedLanguages(&languages);
  base::Time last_updated =
      TranslateManager::GetSupportedLanguagesLastUpdated();

  ListValue* languages_list = new ListValue();
  ListValue* alpha_languages_list = new ListValue();
  for (std::vector<std::string>::iterator it = languages.begin();
       it != languages.end(); ++it) {
    const std::string& lang = *it;
    languages_list->Append(new StringValue(lang));
    if (TranslateManager::IsAlphaLanguage(lang))
      alpha_languages_list->Append(new StringValue(lang));
  }

  dict.Set("languages", languages_list);
  dict.Set("alpha_languages", alpha_languages_list);
  dict.Set("last_updated",
           new base::FundamentalValue(last_updated.ToJsTime()));
  SendMessageToJs("supportedLanguagesUpdated", dict);
}
