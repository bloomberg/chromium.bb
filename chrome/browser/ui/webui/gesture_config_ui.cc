// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/gesture_config_ui.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

/**
 * WebUI for configuring 'gesture.*' preference values used by
 * Chrome's gesture recognition system.
 */
GestureConfigUI::GestureConfigUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://gesture-config source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIGestureConfigHost);

  // Register callback handlers.
  web_ui->RegisterMessageCallback(
      "updatePreferenceValue",
      base::Bind(&GestureConfigUI::UpdatePreferenceValue,
                 base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "resetPreferenceValue",
      base::Bind(&GestureConfigUI::ResetPreferenceValue,
                 base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "setPreferenceValue",
      base::Bind(&GestureConfigUI::SetPreferenceValue,
                 base::Unretained(this)));

  // Add required resources.
  html_source->AddResourcePath("gesture_config.css", IDR_GESTURE_CONFIG_CSS);
  html_source->AddResourcePath("gesture_config.js", IDR_GESTURE_CONFIG_JS);
  html_source->SetDefaultResource(IDR_GESTURE_CONFIG_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}

GestureConfigUI::~GestureConfigUI() {
}

void GestureConfigUI::UpdatePreferenceValue(const base::ListValue* args) {
  std::string pref_name;

  if (!args->GetString(0, &pref_name)) return;

  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  const PrefService::Preference* pref =
      prefs->FindPreference(pref_name.c_str());

  base::StringValue js_pref_name(pref_name);
  base::FundamentalValue js_pref_value(prefs->GetDouble(pref_name.c_str()));
  base::FundamentalValue js_pref_default(pref->IsDefaultValue());

  web_ui()->CallJavascriptFunction(
      "gesture_config.updatePreferenceValueResult",
      js_pref_name,
      js_pref_value,
      js_pref_default);
}

void GestureConfigUI::ResetPreferenceValue(const base::ListValue* args) {
  std::string pref_name;

  if (!args->GetString(0, &pref_name)) return;

  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();

  prefs->ClearPref(pref_name.c_str());
  UpdatePreferenceValue(args);
}

void GestureConfigUI::SetPreferenceValue(const base::ListValue* args) {
  std::string pref_name;
  double value;

  if (!args->GetString(0, &pref_name) || !args->GetDouble(1, &value)) return;

  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();

  const PrefService::Preference* pref =
      prefs->FindPreference(pref_name.c_str());
  switch (pref->GetType()) {
    case base::Value::TYPE_INTEGER:
      prefs->SetInteger(pref_name.c_str(), static_cast<int>(value));
      break;
    case base::Value::TYPE_DOUBLE:
      prefs->SetDouble(pref_name.c_str(), value);
      break;
    default:
      NOTREACHED();
  }
}

