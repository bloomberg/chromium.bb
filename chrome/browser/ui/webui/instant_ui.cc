// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/instant_ui.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace {

content::WebUIDataSource* CreateInstantHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIInstantHost);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("instant.js", IDR_INSTANT_JS);
  source->AddResourcePath("instant.css", IDR_INSTANT_CSS);
  source->SetDefaultResource(IDR_INSTANT_HTML);
  source->UseGzip();
  return source;
}

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
}

void InstantUIMessageHandler::GetPreferenceValue(const base::ListValue* args) {
  std::string pref_name;
  if (!args->GetString(0, &pref_name)) return;

  base::Value pref_name_value(pref_name);
  if (pref_name == prefs::kInstantUIZeroSuggestUrlPrefix) {
    PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
    base::Value arg(prefs->GetString(pref_name.c_str()));
    web_ui()->CallJavascriptFunctionUnsafe(
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
    prefs->SetString(pref_name, value);
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// InstantUI

InstantUI::InstantUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(base::MakeUnique<InstantUIMessageHandler>());

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
