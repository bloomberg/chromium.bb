// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/webui/login/login_container_ui.h"

#include "base/ref_counted_memory.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/browser/chromeos/webui/login/browser/dom_browser.h"
#include "chrome/browser/chromeos/webui/login/login_ui_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "views/screen.h"

static const char kLoginURL[] = "chrome://login";

namespace chromeos {

// LoginContainerUIHTMLSource --------------------------------------------------

LoginContainerUIHTMLSource::LoginContainerUIHTMLSource(
    MessageLoop* message_loop)
    : DataSource(chrome::kChromeUILoginContainerHost, message_loop),
      html_operations_(new HTMLOperationsInterface()) {
}

void LoginContainerUIHTMLSource::StartDataRequest(const std::string& path,
                                                  bool is_off_the_record,
                                                  int request_id) {
  DictionaryValue localized_strings;
  SetFontAndTextDirection(&localized_strings);

  base::StringPiece login_html = html_operations_->GetLoginContainerHTML();
  std::string full_html = html_operations_->GetFullHTML(login_html,
                                                        &localized_strings);
  scoped_refptr<RefCountedBytes> html_bytes(
      html_operations_->CreateHTMLBytes(full_html));
  SendResponse(request_id, (html_bytes.get()));
}

// LoginContainerUIHandler -----------------------------------------------------

LoginContainerUIHandler::LoginContainerUIHandler()
    : browser_operations_(new BrowserOperationsInterface),
      profile_operations_(new ProfileOperationsInterface) {
}

LoginContainerUIHandler::~LoginContainerUIHandler() {
}

WebUIMessageHandler* LoginContainerUIHandler::Attach(WebUI* web_ui) {
  return WebUIMessageHandler::Attach(web_ui);
}

void LoginContainerUIHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(
      "OpenLoginScreen",
      NewCallback(this,
                  &LoginContainerUIHandler::HandleOpenLoginScreen));
}

void LoginContainerUIHandler::HandleOpenLoginScreen(const ListValue* args) {
  Profile* profile = profile_operations_->GetDefaultProfileByPath();
  Browser* current_browser = browser_operations_->GetLoginBrowser(profile);
  Browser* login_screen = DOMBrowser::CreateForDOM(profile);
  login_screen->AddSelectedTabWithURL(GURL(kLoginURL), PageTransition::LINK);
  login_screen->window()->Show();
  current_browser->CloseWindow();
}

// LoginContainerUI ------------------------------------------------------------

LoginContainerUI::LoginContainerUI(TabContents* contents)
    : WebUI(contents) {
  LoginContainerUIHandler* handler = new LoginContainerUIHandler();
  AddMessageHandler(handler->Attach(this));
  LoginContainerUIHTMLSource* html_source =
      new LoginContainerUIHTMLSource(MessageLoop::current());

  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}

}  // namespace chromeos
