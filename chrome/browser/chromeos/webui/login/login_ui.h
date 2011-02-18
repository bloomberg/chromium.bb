// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_LOGIN_UI_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_LOGIN_UI_H_
#pragma once

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/webui/web_ui.h"

class Profile;

namespace chromeos {

class AuthenticatorFacade;
class BrowserOperationsInterface;
class HTMLOperationsInterface;
class ProfileOperationsInterface;

// Boilerplate class that is used to associate the LoginUI code with the URL
// "chrome://login"
class LoginUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  explicit LoginUIHTMLSource(MessageLoop* message_loop);

  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  scoped_ptr<HTMLOperationsInterface> html_operations_;

  DISALLOW_COPY_AND_ASSIGN(LoginUIHTMLSource);
};

// Main LoginUI handling function. It handles the WebUI hooks that are supplied
// for the login page to use for authentication. It passes the key pair form the
// login page to the AuthenticatorFacade. The facade then will call back into
// this class with the result, which will then be used to determine the browser
// behaviour.
class LoginUIHandler : public WebUIMessageHandler,
                       public chromeos::LoginStatusConsumer {
 public:
  LoginUIHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  void HandleAuthenticateUser(const ListValue* args);
  void HandleLaunchIncognito(const ListValue* args);
  void HandleShutdownSystem(const ListValue* args);

  // Overridden from LoginStatusConsumer.
  virtual void OnLoginFailure(const chromeos::LoginFailure& failure);
  virtual void OnLoginSuccess(
      const std::string& username,
      const std::string& password,
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      bool pending_requests);
  virtual void OnOffTheRecordLoginSuccess();

 protected:
  scoped_ptr<AuthenticatorFacade> facade_;
  scoped_ptr<ProfileOperationsInterface> profile_operations_;
  scoped_ptr<BrowserOperationsInterface> browser_operations_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginUIHandler);
};

// Boilerplate class that is used to associate the LoginUI code with the WebUI
// code.
class LoginUI : public WebUI {
 public:
  explicit LoginUI(TabContents* contents);

  // Return the URL for a given search term.
  static const GURL GetLoginURLWithSearchText(const string16& text);

  static RefCountedMemory* GetFaviconResourceBytes();

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_LOGIN_UI_H_
