// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted_memory.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/dom_ui/login/authenticator_facade.h"
#include "chrome/browser/chromeos/dom_ui/login/authenticator_facade_cros.h"
#include "chrome/browser/chromeos/dom_ui/login/authenticator_facade_cros.h"
#include "chrome/browser/chromeos/dom_ui/login/login_ui.h"
#include "chrome/browser/chromeos/dom_ui/login/login_ui_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/url_constants.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
//
// LoginUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

LoginUIHTMLSource::LoginUIHTMLSource(MessageLoop* message_loop)
    : DataSource(chrome::kChromeUILoginHost, message_loop),
      html_operations_(new HTMLOperationsInterface()) {
}

void LoginUIHTMLSource::StartDataRequest(const std::string& path,
                                         bool is_off_the_record,
                                         int request_id) {
  DictionaryValue localized_strings;
  SetFontAndTextDirection(&localized_strings);

  base::StringPiece login_html = html_operations_->GetLoginHTML();
  std::string full_html = html_operations_->GetFullHTML(login_html,
                                                        &localized_strings);
  scoped_refptr<RefCountedBytes> html_bytes(
      html_operations_->CreateHTMLBytes(full_html));
  SendResponse(request_id,
               (html_bytes.get()));
}

////////////////////////////////////////////////////////////////////////////////
//
// LoginUIHandler
//
////////////////////////////////////////////////////////////////////////////////

LoginUIHandler::LoginUIHandler()
    : facade_(new chromeos::AuthenticatorFacadeCros(this)),
      profile_operations_(new ProfileOperationsInterface()),
      browser_operations_(new BrowserOperationsInterface()) {
  facade_->Setup();
}

WebUIMessageHandler* LoginUIHandler::Attach(DOMUI* dom_ui) {
  return WebUIMessageHandler::Attach(dom_ui);
}

void LoginUIHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback(
      "LaunchIncognito",
      NewCallback(this,
                  &LoginUIHandler::HandleLaunchIncognito));
  dom_ui_->RegisterMessageCallback(
      "AuthenticateUser",
      NewCallback(this,
                  &LoginUIHandler::HandleAuthenticateUser));
  dom_ui_->RegisterMessageCallback(
      "ShutdownSystem",
      NewCallback(this,
                  &LoginUIHandler::HandleShutdownSystem));
}

void LoginUIHandler::HandleAuthenticateUser(const ListValue* args) {
  std::string username;
  std::string password;
  size_t expected_size = 2;
  CHECK_EQ(args->GetSize(), expected_size);
  args->GetString(0, &username);
  args->GetString(1, &password);

  Profile* profile = profile_operations_->GetDefaultProfile();
  // For AuthenticateToLogin we are currently not using/supporting tokens and
  // captchas, but the function signature that we inherit from
  // LoginStatusConsumer has two fields for them. We are currently passing in an
  // empty string for these fields by calling the default constructor for the
  // string class.
  facade_->AuthenticateToLogin(profile,
                               username,
                               password,
                               std::string(),
                               std::string());
}

void LoginUIHandler::HandleLaunchIncognito(const ListValue* args) {
  Profile* profile = profile_operations_->GetDefaultProfileByPath();
  Browser* login_browser = browser_operations_->GetLoginBrowser(profile);
  Browser* logged_in = browser_operations_->CreateBrowser(profile);
  logged_in->NewTab();
  logged_in->window()->Show();
  login_browser->CloseWindow();
}

void LoginUIHandler::HandleShutdownSystem(const ListValue* args) {
#if defined(OS_CHROMEOS)
  DCHECK(CrosLibrary::Get()->EnsureLoaded());
  CrosLibrary::Get()->GetPowerLibrary()->RequestShutdown();
#else
  // When were are not running in cros we are just shutting the browser instead
  // of trying to shutdown the system
  Profile* profile = profile_operations_->GetDefaultProfileByPath();
  Browser* browser = browser_operations_->GetLoginBrowser(profile);
  browser->CloseWindow();
#endif
}

void LoginUIHandler::OnLoginFailure(const LoginFailure& failure) {
  Profile* profile = profile_operations_->GetDefaultProfileByPath();
  Browser* login_browser = browser_operations_->GetLoginBrowser(profile);
  login_browser->OpenCurrentURL();
}

void LoginUIHandler::OnLoginSuccess(
    const std::string& username,
    const std::string& password,
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    bool pending_requests) {
  Profile* profile = profile_operations_->GetDefaultProfileByPath();
  Browser* login_browser = browser_operations_->GetLoginBrowser(profile);
  Browser* logged_in = browser_operations_->CreateBrowser(profile);
  logged_in->NewTab();
  logged_in->window()->Show();
  login_browser->CloseWindow();
}

void LoginUIHandler::OnOffTheRecordLoginSuccess() {
}

////////////////////////////////////////////////////////////////////////////////
//
// LoginUI
//
////////////////////////////////////////////////////////////////////////////////
LoginUI::LoginUI(TabContents* contents)
    : DOMUI(contents) {
  LoginUIHandler* handler = new LoginUIHandler();
  AddMessageHandler(handler->Attach(this));
  LoginUIHTMLSource* html_source =
      new LoginUIHTMLSource(MessageLoop::current());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          ChromeURLDataManager::GetInstance(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}

}  // namespace chromeos
