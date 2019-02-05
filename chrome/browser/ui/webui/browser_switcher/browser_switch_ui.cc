// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/browser_switcher/browser_switch_ui.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/dark_mode_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/grit/components_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace {

content::WebUIDataSource* CreateBrowserSwitchUIHTMLSource(
    content::WebUI* web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIBrowserSwitchHost);

  source->AddLocalizedString("description",
                             IDS_ABOUT_BROWSER_SWITCH_DESCRIPTION);
  source->AddLocalizedString("opening", IDS_ABOUT_BROWSER_SWITCH_OPENING);
  source->AddLocalizedString("title", IDS_ABOUT_BROWSER_SWITCH_TITLE);

  source->AddResourcePath("app.html", IDR_BROWSER_SWITCHER_APP_HTML);
  source->AddResourcePath("app.js", IDR_BROWSER_SWITCHER_APP_JS);
  source->AddResourcePath("browser_switch.html",
                          IDR_BROWSER_SWITCHER_BROWSER_SWITCH_HTML);
  source->SetDefaultResource(IDR_BROWSER_SWITCHER_BROWSER_SWITCH_HTML);
  source->UseGzip();

  return source;
}

class BrowserSwitchHandler : public content::WebUIMessageHandler {
 public:
  BrowserSwitchHandler();
  ~BrowserSwitchHandler() override;

  // WebUIMessageHandler
  void RegisterMessages() override;

 private:
  // Launches the given URL in the configured alternative browser. Acts as a
  // bridge for |AlternativeBrowserDriver::TryLaunch()|.
  void HandleLaunchAlternativeBrowser(const base::ListValue* args);

  // Closes the current tab, since calling |window.close()| from JavaScript is
  // not normally allowed.
  void HandleCloseTab(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(BrowserSwitchHandler);
};

BrowserSwitchHandler::BrowserSwitchHandler() = default;
BrowserSwitchHandler::~BrowserSwitchHandler() = default;

void BrowserSwitchHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "launchAlternativeBrowser",
      base::BindRepeating(&BrowserSwitchHandler::HandleLaunchAlternativeBrowser,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "closeTab", base::BindRepeating(&BrowserSwitchHandler::HandleCloseTab,
                                      base::Unretained(this)));
}

void BrowserSwitchHandler::HandleLaunchAlternativeBrowser(
    const base::ListValue* args) {
  // Not yet implemented.
}

void BrowserSwitchHandler::HandleCloseTab(const base::ListValue* args) {
  // Not yet implemented.
}

}  // namespace

BrowserSwitchUI::BrowserSwitchUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  content::WebUIDataSource* data_source =
      CreateBrowserSwitchUIHTMLSource(web_ui);
  DarkModeHandler::Initialize(web_ui, data_source);
  web_ui->AddMessageHandler(std::make_unique<BrowserSwitchHandler>());

  // Set up the about:browser-switch source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, data_source);
}
