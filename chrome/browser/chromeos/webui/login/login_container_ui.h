// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_LOGIN_CONTAINER_UI_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_LOGIN_CONTAINER_UI_H_
#pragma once
#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/webui/login/login_ui_helpers.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "content/browser/webui/web_ui.h"

class Profile;

namespace chromeos {

class BrowserOperationsInterface;
class ProfileOperationsInterface;

// Boilerplate class that is used to associate the LoginContainerUI code with
// the URL "chrome://login-container"
class LoginContainerUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  explicit LoginContainerUIHTMLSource(MessageLoop* message_loop);

  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  scoped_ptr<HTMLOperationsInterface> html_operations_;

  DISALLOW_COPY_AND_ASSIGN(LoginContainerUIHTMLSource);
};

// Main LoginContainerUI handling function. It handles the WebUI hooks that are
// supplied for the page to launch the login container.
class LoginContainerUIHandler : public WebUIMessageHandler {
 public:
  LoginContainerUIHandler();
  virtual ~LoginContainerUIHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  void HandleOpenLoginScreen(const ListValue* args);

 private:
  scoped_ptr<BrowserOperationsInterface> browser_operations_;
  scoped_ptr<ProfileOperationsInterface> profile_operations_;

  DISALLOW_COPY_AND_ASSIGN(LoginContainerUIHandler);
};

// Boilerplate class that is used to associate the LoginContainerUI code with
// the Webui code.
class LoginContainerUI : public WebUI {
 public:
  explicit LoginContainerUI(TabContents* contents);

  // Return the URL for a given search term.
  static const GURL GetLoginURLWithSearchText(const string16& text);

  static RefCountedMemory* GetFaviconResourceBytes();

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginContainerUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_LOGIN_CONTAINER_UI_H_
