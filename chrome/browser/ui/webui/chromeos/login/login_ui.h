// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOGIN_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOGIN_UI_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "content/browser/webui/web_ui.h"

class Profile;

namespace chromeos {

class HTMLOperationsInterface;

// Boilerplate class that is used to associate the LoginUI code with the URL
// "chrome://login"
class LoginUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  explicit LoginUIHTMLSource(MessageLoop* message_loop);

  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const;

 private:
  scoped_ptr<HTMLOperationsInterface> html_operations_;

  DISALLOW_COPY_AND_ASSIGN(LoginUIHTMLSource);
};

class LoginUIHandler;
class LoginUIHandlerDelegate {
 public:
  LoginUIHandlerDelegate()
      : login_handler_(NULL) { }
  // Sign in using |username| and |password| specified.
  // Used for both known and new users.
  virtual void Login(const std::string& username,
                     const std::string& password) = 0;

  // Sign in into Guest session.
  virtual void LoginAsGuest() = 0;
  // Let the delegate know about the handler it is supposed to be using.
  virtual void set_login_handler(LoginUIHandler* login_handler) {
    login_handler_ = login_handler;
  }
 protected:
  // Reference to the DOM handling layer for the login screen
  LoginUIHandler* login_handler_;

  virtual ~LoginUIHandlerDelegate();
};

// Main LoginUI handling function. It handles the WebUI hooks that are supplied
// for the login page to use for authentication.
class LoginUIHandler : public WebUIMessageHandler {
 public:
  LoginUIHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  void HandleAuthenticateUser(const ListValue* args);
  void HandleLaunchIncognito(const ListValue* args);
  void HandleShutdownSystem(const ListValue* args);
  void ClearAndEnablePassword();

 protected:
  LoginUIHandlerDelegate* delegate_;

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

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOGIN_UI_H_
