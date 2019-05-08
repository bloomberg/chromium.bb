// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/browser_switcher/browser_switch_ui.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chrome/browser/browser_switcher/alternative_browser_driver.h"
#include "chrome/browser/browser_switcher/browser_switcher_service.h"
#include "chrome/browser/browser_switcher/browser_switcher_service_factory.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/dark_mode_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/grit/components_resources.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

void GotoNewTabPage(content::WebContents* web_contents) {
  GURL url(chrome::kChromeUINewTabURL);
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
  web_contents->OpenURL(params);
}

// Returns true if there's only 1 tab left open in this profile. Incognito
// window tabs count as the same profile.
bool IsLastTab(const Profile* profile) {
  profile = profile->GetOriginalProfile();
  int tab_count = 0;
  for (const Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->GetOriginalProfile() != profile)
      continue;
    tab_count += browser->tab_strip_model()->count();
    if (tab_count > 1)
      return false;
  }
  return true;
}

browser_switcher::BrowserSwitcherService* GetBrowserSwitcherService(
    content::WebUI* web_ui) {
  return browser_switcher::BrowserSwitcherServiceFactory::GetForBrowserContext(
      web_ui->GetWebContents()->GetBrowserContext());
}

content::WebUIDataSource* CreateBrowserSwitchUIHTMLSource(
    content::WebUI* web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIBrowserSwitchHost);

  auto* service = GetBrowserSwitcherService(web_ui);
  source->AddInteger("launchDelay", service->prefs().GetDelay());

  std::string browser_name = service->driver()->GetBrowserName();
  source->AddString("browserName", browser_name);

  if (browser_name.empty()) {
    // Browser name could not be auto-detected. Say "alternative browser"
    // instead of naming the browser.
    source->AddLocalizedString(
        "countdownTitle",
        IDS_ABOUT_BROWSER_SWITCH_COUNTDOWN_TITLE_UNKNOWN_BROWSER);
    source->AddLocalizedString(
        "description", IDS_ABOUT_BROWSER_SWITCH_DESCRIPTION_UNKNOWN_BROWSER);
    source->AddLocalizedString(
        "errorTitle", IDS_ABOUT_BROWSER_SWITCH_ERROR_TITLE_UNKNOWN_BROWSER);
    source->AddLocalizedString(
        "genericError", IDS_ABOUT_BROWSER_SWITCH_GENERIC_ERROR_UNKNOWN_BROWSER);
    source->AddLocalizedString(
        "openingTitle", IDS_ABOUT_BROWSER_SWITCH_OPENING_TITLE_UNKNOWN_BROWSER);
  } else {
    // Browser name was auto-detected. Name it in the text.
    source->AddLocalizedString(
        "countdownTitle",
        IDS_ABOUT_BROWSER_SWITCH_COUNTDOWN_TITLE_KNOWN_BROWSER);
    source->AddLocalizedString(
        "description", IDS_ABOUT_BROWSER_SWITCH_DESCRIPTION_KNOWN_BROWSER);
    source->AddLocalizedString(
        "errorTitle", IDS_ABOUT_BROWSER_SWITCH_ERROR_TITLE_KNOWN_BROWSER);
    source->AddLocalizedString(
        "genericError", IDS_ABOUT_BROWSER_SWITCH_GENERIC_ERROR_KNOWN_BROWSER);
    source->AddLocalizedString(
        "openingTitle", IDS_ABOUT_BROWSER_SWITCH_OPENING_TITLE_KNOWN_BROWSER);
  }

  source->AddLocalizedString("protocolError",
                             IDS_ABOUT_BROWSER_SWITCH_PROTOCOL_ERROR);
  source->AddLocalizedString("title", IDS_ABOUT_BROWSER_SWITCH_TITLE);

  source->AddResourcePath("app.html", IDR_BROWSER_SWITCHER_APP_HTML);
  source->AddResourcePath("app.js", IDR_BROWSER_SWITCHER_APP_JS);
  source->AddResourcePath("browser_switch.html",
                          IDR_BROWSER_SWITCHER_BROWSER_SWITCH_HTML);
  source->AddResourcePath("browser_switcher_proxy.html",
                          IDR_BROWSER_SWITCHER_BROWSER_SWITCHER_PROXY_HTML);
  source->AddResourcePath("browser_switcher_proxy.js",
                          IDR_BROWSER_SWITCHER_BROWSER_SWITCHER_PROXY_JS);

  source->SetDefaultResource(IDR_BROWSER_SWITCHER_BROWSER_SWITCH_HTML);
  source->SetJsonPath("strings.js");

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
  // bridge for |AlternativeBrowserDriver::TryLaunch()|. Then, if that succeeds,
  // closes the current tab.
  //
  // If it fails, the JavaScript promise is rejected. If it succeeds, the
  // JavaScript promise is not resolved, because we close the tab anyways.
  void HandleLaunchAlternativeBrowserAndCloseTab(const base::ListValue* args);

  // Navigates to the New Tab Page.
  void HandleGotoNewTabPage(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(BrowserSwitchHandler);
};

BrowserSwitchHandler::BrowserSwitchHandler() = default;
BrowserSwitchHandler::~BrowserSwitchHandler() = default;

void BrowserSwitchHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "launchAlternativeBrowserAndCloseTab",
      base::BindRepeating(
          &BrowserSwitchHandler::HandleLaunchAlternativeBrowserAndCloseTab,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "gotoNewTabPage",
      base::BindRepeating(&BrowserSwitchHandler::HandleGotoNewTabPage,
                          base::Unretained(this)));
}

void BrowserSwitchHandler::HandleLaunchAlternativeBrowserAndCloseTab(
    const base::ListValue* args) {
  DCHECK(args);
  AllowJavascript();

  std::string callback_id = args->GetList()[0].GetString();
  std::string url_spec = args->GetList()[1].GetString();
  GURL url(url_spec);

  auto* service = GetBrowserSwitcherService(web_ui());
  bool should_switch = service->sitelist()->ShouldSwitch(url);
  if (!url.is_valid() || !should_switch) {
    // This URL shouldn't open in an alternative browser. Abort launch, because
    // something weird is going on (e.g. race condition from a new sitelist
    // being loaded).
    RejectJavascriptCallback(args->GetList()[0], base::Value());
    return;
  }

  bool success;
  {
    SCOPED_UMA_HISTOGRAM_TIMER("BrowserSwitcher.LaunchTime");
    success = service->driver()->TryLaunch(url);
    UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.LaunchSuccess", success);
  }

  if (!success) {
    RejectJavascriptCallback(args->GetList()[0], base::Value());
    return;
  }

  auto* profile = Profile::FromWebUI(web_ui());

  if (service->prefs().KeepLastTab() && IsLastTab(profile)) {
    GotoNewTabPage(web_ui()->GetWebContents());
  } else {
    // We don't need to resolve the promise, because the tab will close anyways.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&content::WebContents::ClosePage,
                       base::Unretained(web_ui()->GetWebContents())));
  }
}

void BrowserSwitchHandler::HandleGotoNewTabPage(const base::ListValue* args) {
  GotoNewTabPage(web_ui()->GetWebContents());
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
