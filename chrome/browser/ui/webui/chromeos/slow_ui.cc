// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/slow_ui.h"

#include <string>

#include "base/bind.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

using content::WebUIMessageHandler;

namespace {

// JS API callbacks names.
const char kJsApiDisableTracing[] = "disableTracing";
const char kJsApiEnableTracing[] = "enableTracing";
const char kJsApiLoadComplete[] = "loadComplete";

// Page JS API function names.
const char kJsApiTracingPrefChanged[] = "options.Slow.tracingPrefChanged";

}  // namespace

namespace chromeos {

content::WebUIDataSource* CreateSlowUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISlowHost);

  source->SetUseJsonJSFormatV2();
  source->AddLocalizedString("slowDisable", IDS_SLOW_DISABLE);
  source->AddLocalizedString("slowEnable", IDS_SLOW_ENABLE);
  source->AddLocalizedString("slowDescription", IDS_SLOW_DESCRIPTION);
  source->AddLocalizedString("slowWarning", IDS_SLOW_WARNING);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("slow.js", IDR_SLOW_JS);
  source->SetDefaultResource(IDR_SLOW_HTML);
  return source;
}

// The handler for Javascript messages related to the "slow" view.
class SlowHandler : public WebUIMessageHandler {
 public:
  explicit SlowHandler(Profile* profile);
  virtual ~SlowHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  void UpdatePage();

  // Handlers for JS WebUI messages.
  void HandleDisable(const base::ListValue* args);
  void HandleEnable(const base::ListValue* args);
  void LoadComplete(const base::ListValue* args);

  Profile* profile_;
  scoped_ptr<PrefChangeRegistrar> user_pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(SlowHandler);
};

// SlowHandler ------------------------------------------------------------

SlowHandler::SlowHandler(Profile* profile) : profile_(profile) {
}

SlowHandler::~SlowHandler() {
}

void SlowHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(kJsApiDisableTracing,
      base::Bind(&SlowHandler::HandleDisable, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiEnableTracing,
      base::Bind(&SlowHandler::HandleEnable, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiLoadComplete,
      base::Bind(&SlowHandler::LoadComplete, base::Unretained(this)));

  user_pref_registrar_.reset(new PrefChangeRegistrar);
  user_pref_registrar_->Init(profile_->GetPrefs());
  user_pref_registrar_->Add(prefs::kPerformanceTracingEnabled,
                            base::Bind(&SlowHandler::UpdatePage,
                                       base::Unretained(this)));
}

void SlowHandler::HandleDisable(const base::ListValue* args) {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kPerformanceTracingEnabled, false);
}

void SlowHandler::HandleEnable(const base::ListValue* args) {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kPerformanceTracingEnabled, true);
}

void SlowHandler::LoadComplete(const base::ListValue* args) {
  UpdatePage();
}

void SlowHandler::UpdatePage() {
  PrefService* pref_service = profile_->GetPrefs();
  bool enabled = pref_service->GetBoolean(prefs::kPerformanceTracingEnabled);
  base::FundamentalValue pref_value(enabled);
  web_ui()->CallJavascriptFunction(kJsApiTracingPrefChanged, pref_value);
}

// SlowUI -----------------------------------------------------------------

SlowUI::SlowUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  SlowHandler* handler = new SlowHandler(profile);
  web_ui->AddMessageHandler(handler);

  // Set up the chrome://slow/ source.
  content::WebUIDataSource::Add(profile, CreateSlowUIHTMLSource());
}

}  // namespace chromeos

