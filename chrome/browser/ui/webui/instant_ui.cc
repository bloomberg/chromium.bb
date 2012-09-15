// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/instant_ui.h"

#include "base/bind.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"

namespace {

ChromeWebUIDataSource* CreateInstantHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIInstantHost);

  source->set_json_path("strings.js");
  source->add_resource_path("instant.js", IDR_INSTANT_JS);
  source->add_resource_path("instant.css", IDR_INSTANT_CSS);
  source->set_default_resource(IDR_INSTANT_HTML);
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
  virtual ~InstantUIMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  static int slow_animation_scale_factor() {
    return slow_animation_scale_factor_;
  }

 private:
  void GetPreferenceValue(const base::ListValue* args);
  void SetPreferenceValue(const base::ListValue* args);

  // Slows down Instant animations by a time factor.
  static int slow_animation_scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(InstantUIMessageHandler);
};

// static
int InstantUIMessageHandler::slow_animation_scale_factor_ = 1;

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

  base::StringValue pref_name_value(pref_name);
  if (pref_name == prefs::kInstantAnimationScaleFactor) {
    double value = 0.0;
#if defined(TOOLKIT_VIEWS)
    value = slow_animation_scale_factor_;
#endif
    base::FundamentalValue arg(value);
    web_ui()->CallJavascriptFunction(
        "instantConfig.getPreferenceValueResult", pref_name_value, arg);
  } else if (pref_name == prefs::kExperimentalZeroSuggestUrlPrefix) {
    PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
    base::StringValue arg(prefs->GetString(pref_name.c_str()));
    web_ui()->CallJavascriptFunction(
        "instantConfig.getPreferenceValueResult", pref_name_value, arg);
  }
}

void InstantUIMessageHandler::SetPreferenceValue(const base::ListValue* args) {
  std::string pref_name;
  if (!args->GetString(0, &pref_name)) return;

  if (pref_name == prefs::kInstantAnimationScaleFactor) {
    double value;
    if (!args->GetDouble(1, &value)) return;
#if defined(TOOLKIT_VIEWS)
    // Clamp to something reasonable.
    value = std::max(0.1, std::min(value, 10.0));
    slow_animation_scale_factor_ = static_cast<int>(value);
#else
    NOTIMPLEMENTED();
#endif
  } else if (pref_name == prefs::kExperimentalZeroSuggestUrlPrefix) {
    std::string value;
    if (!args->GetString(1, &value)) return;
    PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
    prefs->SetString(pref_name.c_str(), value);
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// InstantUI

InstantUI::InstantUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new InstantUIMessageHandler());

  // Set up the chrome://instant/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, CreateInstantHTMLSource());
}

// static
int InstantUI::GetSlowAnimationScaleFactor() {
  return InstantUIMessageHandler::slow_animation_scale_factor();
}
