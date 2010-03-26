// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/eula_ui.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "webkit/glue/plugins/plugin_list.h"

namespace {

///////////////////////////////////////////////////////////////////////////////
//
// EulaHTMLSource
//
///////////////////////////////////////////////////////////////////////////////

class EulaUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  EulaUIHTMLSource()
      : DataSource(chrome::kChromeUIEulaHost, MessageLoop::current()) {}

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~EulaUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(EulaUIHTMLSource);
};

void EulaUIHTMLSource::StartDataRequest(const std::string& path,
                                        bool is_off_the_record,
                                        int request_id) {
  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);

  if (path == chrome::kChromeUIEulaAuthorizeFlashPath) {
    // Strings used in the JsTemplate file.
    // Since this is only temporary, don't provide localized strings.
    // (Yes, it'll be messed up for RTL, but only as messed up as if we had
    // strings in the .grd but no translations.)
    DictionaryValue strings;
    strings.SetString(L"title", L"Adobe Flash Player License Agreement");
    strings.SetString(L"acceptButton", L"Accept and Enable");
    strings.SetString(L"declineButton", L"Not Right Now");
    strings.SetString(L"eulaTitle", L"Adobe Flash Player for Google Chrome");
    strings.SetString(L"eulaDesc",
        L"By enabling the integrated Adobe Flash Player for Google Chrome, "
        L"you accept the <a href=\"http://www.adobe.com/products/eulas/"
        L"players/flash/\" target=\"_blank\">Software License Agreement</a> "
        L"for Adobe Flash Player.");

    SetFontAndTextDirection(&strings);

    static const base::StringPiece eula_html(
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_EULA_HTML));
    const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
        eula_html, &strings);

    html_bytes->data.resize(full_html.size());
    std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());
  }

  SendResponse(request_id, html_bytes);
}

////////////////////////////////////////////////////////////////////////////////
//
// EulaDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages.
class EulaDOMHandler : public DOMMessageHandler {
 public:
  EulaDOMHandler() {}
  virtual ~EulaDOMHandler() {}

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // Callbacks for the "acceptEula" and "declineEula" messages.
  void HandleAcceptEulaMessage(const Value* value);
  void HandleDeclineEulaMessage(const Value* value);

 private:
  void NavigateToNewTabPage();

  DISALLOW_COPY_AND_ASSIGN(EulaDOMHandler);
};

void EulaDOMHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("acceptEula",
      NewCallback(this, &EulaDOMHandler::HandleAcceptEulaMessage));
  dom_ui_->RegisterMessageCallback("declineEula",
      NewCallback(this, &EulaDOMHandler::HandleDeclineEulaMessage));
}

void EulaDOMHandler::HandleAcceptEulaMessage(const Value* value) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  FilePath flash_path;
  if (command_line->HasSwitch(switches::kEnableInternalFlash) &&
      PathService::Get(chrome::FILE_FLASH_PLUGIN, &flash_path)) {
    dom_ui_->GetProfile()->GetPrefs()->SetBoolean(
        prefs::kPluginsFlashAuthorized, true);
    NPAPI::PluginList::Singleton()->EnablePlugin(FilePath(flash_path));
  } else {
    LOG(WARNING) << "Internal Flash Player not enabled or not available.";
  }

  NavigateToNewTabPage();
}

void EulaDOMHandler::HandleDeclineEulaMessage(const Value* value) {
  // Be generous in recording declines.
  dom_ui_->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kPluginsFlashAuthorized, false);
  FilePath flash_path;
  if (PathService::Get(chrome::FILE_FLASH_PLUGIN, &flash_path))
    NPAPI::PluginList::Singleton()->DisablePlugin(FilePath(flash_path));

  NavigateToNewTabPage();
}

void EulaDOMHandler::NavigateToNewTabPage() {
  TabContents* tab_contents = dom_ui_->tab_contents();
  tab_contents->OpenURL(GURL(chrome::kChromeUINewTabURL), GURL(),
                        CURRENT_TAB, PageTransition::GENERATED);
  if (Browser* browser =
          Browser::GetBrowserForController(&tab_contents->controller(), NULL)) {
    browser->FocusLocationBar();
  }
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// EulaUI
//
///////////////////////////////////////////////////////////////////////////////

EulaUI::EulaUI(TabContents* contents) : DOMUI(contents) {
  AddMessageHandler((new EulaDOMHandler())->Attach(this));

  EulaUIHTMLSource* html_source = new EulaUIHTMLSource();

  // Set up the chrome://eula/ source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}
