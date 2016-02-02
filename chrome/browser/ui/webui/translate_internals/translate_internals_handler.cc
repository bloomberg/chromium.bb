// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/translate_internals/translate_internals_handler.h"

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_error_details.h"
#include "components/translate/core/browser/translate_event_details.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

TranslateInternalsHandler::TranslateInternalsHandler() {
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                              content::NotificationService::AllSources());

  error_subscription_ =
      translate::TranslateManager::RegisterTranslateErrorCallback(
          base::Bind(&TranslateInternalsHandler::OnTranslateError,
                     base::Unretained(this)));

  translate::TranslateLanguageList* language_list =
      translate::TranslateDownloadManager::GetInstance()->language_list();
  if (!language_list) {
    NOTREACHED();
    return;
  }

  event_subscription_ = language_list->RegisterEventCallback(base::Bind(
      &TranslateInternalsHandler::OnTranslateEvent, base::Unretained(this)));
}

TranslateInternalsHandler::~TranslateInternalsHandler() {
  // |event_subscription_| and |error_subscription_| are deleted automatically
  // and un-register the callbacks automatically.
}

void TranslateInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("removePrefItem", base::Bind(
      &TranslateInternalsHandler::OnRemovePrefItem, base::Unretained(this)));
  web_ui()->RegisterMessageCallback("requestInfo", base::Bind(
      &TranslateInternalsHandler::OnRequestInfo, base::Unretained(this)));
}

void TranslateInternalsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED, type);
  const translate::LanguageDetectionDetails* language_detection_details =
      content::Details<const translate::LanguageDetectionDetails>(details)
          .ptr();
  content::WebContents* web_contents =
      content::Source<content::WebContents>(source).ptr();

  if (web_contents->GetBrowserContext()->IsOffTheRecord() ||
      !TranslateService::IsTranslatableURL(language_detection_details->url)) {
    return;
  }

  base::DictionaryValue dict;
  dict.Set(
      "time",
      new base::FundamentalValue(language_detection_details->time.ToJsTime()));
  dict.Set("url",
           new base::StringValue(language_detection_details->url.spec()));
  dict.Set("content_language",
           new base::StringValue(language_detection_details->content_language));
  dict.Set("cld_language",
           new base::StringValue(language_detection_details->cld_language));
  dict.Set(
      "is_cld_reliable",
      new base::FundamentalValue(language_detection_details->is_cld_reliable));
  dict.Set(
      "has_notranslate",
      new base::FundamentalValue(language_detection_details->has_notranslate));
  dict.Set(
      "html_root_language",
      new base::StringValue(language_detection_details->html_root_language));
  dict.Set("adopted_language",
           new base::StringValue(language_detection_details->adopted_language));
  dict.Set("content",
           new base::StringValue(language_detection_details->contents));
  SendMessageToJs("languageDetectionInfoAdded", dict);
}

void TranslateInternalsHandler::OnTranslateError(
    const translate::TranslateErrorDetails& details) {
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
    const translate::TranslateEventDetails& details) {
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
  scoped_ptr<translate::TranslatePrefs> translate_prefs(
      ChromeTranslateClient::CreateTranslatePrefs(prefs));

  std::string pref_name;
  if (!args->GetString(0, &pref_name))
    return;

  if (pref_name == "blocked_languages") {
    std::string language;
    if (!args->GetString(1, &language))
      return;
    translate_prefs->UnblockLanguage(language);
  } else if (pref_name == "language_blacklist") {
    std::string language;
    if (!args->GetString(1, &language))
      return;
    translate_prefs->RemoveLanguageFromLegacyBlacklist(language);
  } else if (pref_name == "site_blacklist") {
    std::string site;
    if (!args->GetString(1, &site))
      return;
    translate_prefs->RemoveSiteFromBlacklist(site);
  } else if (pref_name == "whitelists") {
    std::string from, to;
    if (!args->GetString(1, &from))
      return;
    if (!args->GetString(2, &to))
      return;
    translate_prefs->RemoveLanguagePairFromWhitelist(from, to);
  } else if (pref_name == "too_often_denied") {
    translate_prefs->ResetDenialState();
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

  static const char* keys[] = {
    prefs::kEnableTranslate,
    translate::TranslatePrefs::kPrefTranslateBlockedLanguages,
    translate::TranslatePrefs::kPrefTranslateSiteBlacklist,
    translate::TranslatePrefs::kPrefTranslateWhitelists,
    translate::TranslatePrefs::kPrefTranslateDeniedCount,
    translate::TranslatePrefs::kPrefTranslateAcceptedCount,
    translate::TranslatePrefs::kPrefTranslateLastDeniedTimeForLanguage,
    translate::TranslatePrefs::kPrefTranslateTooOftenDeniedForLanguage,
  };
  for (const char* key : keys) {
    const PrefService::Preference* pref = prefs->FindPreference(key);
    if (pref)
      dict.Set(key, pref->GetValue()->DeepCopy());
  }

  SendMessageToJs("prefsUpdated", dict);
}

void TranslateInternalsHandler::SendSupportedLanguagesToJs() {
  base::DictionaryValue dict;

  std::vector<std::string> languages;
  translate::TranslateDownloadManager::GetSupportedLanguages(&languages);
  base::Time last_updated =
      translate::TranslateDownloadManager::GetSupportedLanguagesLastUpdated();

  base::ListValue* languages_list = new base::ListValue();
  base::ListValue* alpha_languages_list = new base::ListValue();
  for (std::vector<std::string>::iterator it = languages.begin();
       it != languages.end(); ++it) {
    const std::string& lang = *it;
    languages_list->Append(new base::StringValue(lang));
    if (translate::TranslateDownloadManager::IsAlphaLanguage(lang))
      alpha_languages_list->Append(new base::StringValue(lang));
  }

  dict.Set("languages", languages_list);
  dict.Set("alpha_languages", alpha_languages_list);
  dict.Set("last_updated",
           new base::FundamentalValue(last_updated.ToJsTime()));
  SendMessageToJs("supportedLanguagesUpdated", dict);
}
