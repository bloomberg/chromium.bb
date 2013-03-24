// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/app_launch_ui.h"

#include "base/values.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/web_ui_util.h"

namespace chromeos {

namespace {

content::WebUIDataSource* CreateWebUIDataSource() {
  content::WebUIDataSource* source =
        content::WebUIDataSource::Create(chrome::kChromeUIAppLaunchHost);
  source->SetDefaultResource(IDR_APP_LAUNCH_SPLASH_HTML);
  source->SetUseJsonJSFormatV2();
  source->AddLocalizedString("appStartMessage",
                             net::NetworkChangeNotifier::IsOffline() ?
                                 IDS_APP_START_NETWORK_WAIT_MESSAGE :
                                 IDS_APP_START_APP_WAIT_MESSAGE);
  source->AddLocalizedString("productName", IDS_SHORT_PRODUCT_NAME);

  const string16 product_os_name =
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME);
  source->AddString(
      "shortcutInfo",
      l10n_util::GetStringFUTF16(IDS_APP_START_BAILOUT_SHORTCUT_FORMAT,
                                 product_os_name));

  // TODO(xiyuan): Wire this with settings when that change lands.
  source->AddBoolean("shortcutEnabled", true);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("app_launch.js", IDR_APP_LAUNCH_SPLASH_JS);
  return source;
}

}  // namespace

class AppLaunchUI::AppLaunchUIHandler : public content::WebUIMessageHandler {
 public:
  AppLaunchUIHandler() : initialized_(false) {}
  virtual ~AppLaunchUIHandler() {}

  void SetLaunchText(const std::string& text) {
    text_ = text;
    if (initialized_)
      SendLaunchText();
  }

  // content::WebUIMessageHandler overrides:
  virtual void RegisterMessages() OVERRIDE {
    web_ui()->RegisterMessageCallback("initialize",
        base::Bind(&AppLaunchUIHandler::HandleInitialize,
                   base::Unretained(this)));
  }

 private:
  void SendAppInfo(const std::string& app_id) {
    KioskAppManager::App app;
    KioskAppManager::Get()->GetApp(app_id, &app);

    if (app.name.empty() || app.icon.isNull())
      return;

    base::DictionaryValue app_info;
    app_info.SetString("name", app.name);
    app_info.SetString("iconURL", webui::GetBitmapDataUrl(*app.icon.bitmap()));

    web_ui()->CallJavascriptFunction("updateApp", app_info);
  }

  void SendLaunchText() {
    web_ui()->CallJavascriptFunction("updateMessage", base::StringValue(text_));
  }

  // JS callbacks.
  void HandleInitialize(const base::ListValue* args) {
    initialized_ = true;

    std::string app_id;
    if (args->GetString(0, &app_id) && !app_id.empty())
      SendAppInfo(app_id);

    SendLaunchText();
  }

  bool initialized_;
  std::string text_;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchUIHandler);
};

AppLaunchUI::AppLaunchUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      handler_(NULL) {
  Profile* profile = Profile::FromWebUI(web_ui);
  // Set up the chrome://theme/ source, for Chrome logo.
  ThemeSource* theme = new ThemeSource(profile);
  content::URLDataSource::Add(profile, theme);
  // Add data source.
  content::WebUIDataSource::Add(profile, CreateWebUIDataSource());

  handler_ = new AppLaunchUIHandler;
  web_ui->AddMessageHandler(handler_);
}

void AppLaunchUI::SetLaunchText(const std::string& text) {
  handler_->SetLaunchText(text);
}

}  // namespace chromeos
