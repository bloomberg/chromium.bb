// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/instant_ui.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/search/instant_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"

namespace {

content::WebUIDataSource* CreateInstantHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIInstantHost);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("instant.js", IDR_INSTANT_JS);
  source->AddResourcePath("instant.css", IDR_INSTANT_CSS);
  source->SetDefaultResource(IDR_INSTANT_HTML);
  return source;
}

#if !defined(OS_ANDROID)
std::string FormatTime(int64_t time) {
  base::Time::Exploded exploded;
  base::Time::FromInternalValue(time).UTCExplode(&exploded);
  return base::StringPrintf("%04d-%02d-%02d %02d:%02d:%02d.%03d",
      exploded.year, exploded.month, exploded.day_of_month,
      exploded.hour, exploded.minute, exploded.second, exploded.millisecond);
}
#endif  // !defined(OS_ANDROID)

// This class receives JavaScript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class InstantUIMessageHandler
    : public content::WebUIMessageHandler,
      public base::SupportsWeakPtr<InstantUIMessageHandler> {
 public:
  InstantUIMessageHandler();
  ~InstantUIMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void GetPreferenceValue(const base::ListValue* args);
  void SetPreferenceValue(const base::ListValue* args);
  void GetDebugInfo(const base::ListValue* value);
  void ClearDebugInfo(const base::ListValue* value);

  DISALLOW_COPY_AND_ASSIGN(InstantUIMessageHandler);
};

InstantUIMessageHandler::InstantUIMessageHandler() {}

InstantUIMessageHandler::~InstantUIMessageHandler() {}

void InstantUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getPreferenceValue",
      base::Bind(&InstantUIMessageHandler::GetPreferenceValue,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setPreferenceValue",
      base::Bind(&InstantUIMessageHandler::SetPreferenceValue,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDebugInfo",
      base::Bind(&InstantUIMessageHandler::GetDebugInfo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearDebugInfo",
      base::Bind(&InstantUIMessageHandler::ClearDebugInfo,
                 base::Unretained(this)));
}

void InstantUIMessageHandler::GetPreferenceValue(const base::ListValue* args) {
  std::string pref_name;
  if (!args->GetString(0, &pref_name)) return;

  base::StringValue pref_name_value(pref_name);
  if (pref_name == prefs::kInstantUIZeroSuggestUrlPrefix) {
    PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
    base::StringValue arg(prefs->GetString(pref_name.c_str()));
    web_ui()->CallJavascriptFunction(
        "instantConfig.getPreferenceValueResult", pref_name_value, arg);
  }
}

void InstantUIMessageHandler::SetPreferenceValue(const base::ListValue* args) {
  std::string pref_name;
  if (!args->GetString(0, &pref_name)) return;

  if (pref_name == prefs::kInstantUIZeroSuggestUrlPrefix) {
    std::string value;
    if (!args->GetString(1, &value))
      return;
    PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
    prefs->SetString(pref_name.c_str(), value);
  }
}

void InstantUIMessageHandler::GetDebugInfo(const base::ListValue* args) {
#if !defined(OS_ANDROID)
  typedef std::pair<int64_t, std::string> DebugEvent;

  if (!web_ui()->GetWebContents())
    return;
  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  if (!browser || !browser->instant_controller())
    return;

  InstantController* instant = browser->instant_controller()->instant();
  const std::list<DebugEvent>& events = instant->debug_events();

  base::DictionaryValue data;
  base::ListValue* entries = new base::ListValue();
  for (std::list<DebugEvent>::const_iterator it = events.begin();
       it != events.end(); ++it) {
    base::DictionaryValue* entry = new base::DictionaryValue();
    entry->SetString("time", FormatTime(it->first));
    entry->SetString("text", it->second);
    entries->Append(entry);
  }
  data.Set("entries", entries);

  web_ui()->CallJavascriptFunction("instantConfig.getDebugInfoResult", data);
#endif
}

void InstantUIMessageHandler::ClearDebugInfo(const base::ListValue* args) {
#if !defined(OS_ANDROID)
  if (!web_ui()->GetWebContents())
    return;
  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  if (!browser || !browser->instant_controller())
    return;

  browser->instant_controller()->instant()->ClearDebugEvents();
#endif
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// InstantUI

InstantUI::InstantUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new InstantUIMessageHandler());

  // Set up the chrome://instant/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateInstantHTMLSource());
}

// static
void InstantUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kInstantUIZeroSuggestUrlPrefix,
                               std::string());
}
