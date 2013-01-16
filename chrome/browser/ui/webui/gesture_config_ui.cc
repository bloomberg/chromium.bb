// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/gesture_config_ui.h"

#include "base/values.h"
#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

/**
 * WebUI for configuring 'gesture.*' preference values used by
 * Chrome's gesture recognition system.
 */
GestureConfigUI::GestureConfigUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://gesture-config source.
  ChromeWebUIDataSource* html_source =
      new ChromeWebUIDataSource(chrome::kChromeUIGestureConfigHost);

  // Register callback handlers.
  web_ui->RegisterMessageCallback(
      "getPreferenceValue",
      base::Bind(&GestureConfigUI::GetPreferenceValue,
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
  html_source->add_resource_path("gesture_config.css", IDR_GESTURE_CONFIG_CSS);
  html_source->add_resource_path("gesture_config.js", IDR_GESTURE_CONFIG_JS);
  html_source->set_default_resource(IDR_GESTURE_CONFIG_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSourceImpl(profile, html_source);
}

GestureConfigUI::~GestureConfigUI() {
}

void GestureConfigUI::GetPreferenceValue(const base::ListValue* args) {
  std::string pref_name;

  if (!args->GetString(0, &pref_name)) return;

  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  base::StringValue js_pref_name(pref_name);
  base::FundamentalValue js_pref_value(prefs->GetDouble(pref_name.c_str()));

  web_ui()->CallJavascriptFunction(
      "gesture_config.getPreferenceValueResult",
      js_pref_name,
      js_pref_value);
}

void GestureConfigUI::ResetPreferenceValue(const base::ListValue* args) {
  std::string pref_name;

  if (!args->GetString(0, &pref_name)) return;

  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  base::StringValue js_pref_name(pref_name);
  double d;
  if (prefs->GetDefaultPrefValue(pref_name.c_str())->GetAsDouble(&d)) {
    base::FundamentalValue js_pref_value(d);
    const PrefService::Preference* pref =
        prefs->FindPreference(pref_name.c_str());
    switch (pref->GetType()) {
      case base::Value::TYPE_INTEGER:
        prefs->SetInteger(pref_name.c_str(), static_cast<int>(d));
        break;
      case base::Value::TYPE_DOUBLE:
        prefs->SetDouble(pref_name.c_str(), d);
        break;
      default:
        NOTREACHED();
    }
    web_ui()->CallJavascriptFunction(
        "gesture_config.getPreferenceValueResult",
        js_pref_name,
        js_pref_value);
  }
}

void GestureConfigUI::SetPreferenceValue(const base::ListValue* args) {
  std::string pref_name;
  double value;

  if (!args->GetString(0, &pref_name) || !args->GetDouble(1, &value)) return;

  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

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

