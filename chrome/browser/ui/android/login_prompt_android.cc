// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_prompt.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/android/chrome_http_auth_handler.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/auth.h"
#include "ui/base/android/window_android.h"

using content::BrowserThread;
using net::URLRequest;
using net::AuthChallengeInfo;

class LoginHandlerAndroid : public LoginHandler {
 public:
  LoginHandlerAndroid(AuthChallengeInfo* auth_info, URLRequest* request)
      : LoginHandler(auth_info, request) {
  }

  // LoginHandler methods:

  virtual void OnAutofillDataAvailable(
      const base::string16& username,
      const base::string16& password) OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(chrome_http_auth_handler_.get() != NULL);
    chrome_http_auth_handler_->OnAutofillDataAvailable(
        username, password);
  }
  virtual void OnLoginModelDestroying() OVERRIDE {}

  virtual void BuildViewForPasswordManager(
      password_manager::PasswordManager* manager,
      const base::string16& explanation) OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Get pointer to TabAndroid
    content::WebContents* web_contents = GetWebContentsForLogin();
    CHECK(web_contents);
    WindowAndroidHelper* window_helper = WindowAndroidHelper::FromWebContents(
        web_contents);

    // Notify WindowAndroid that HTTP authentication is required.
    if (window_helper->GetWindowAndroid()) {
      chrome_http_auth_handler_.reset(new ChromeHttpAuthHandler(explanation));
      chrome_http_auth_handler_->Init();
      chrome_http_auth_handler_->SetObserver(this);
      chrome_http_auth_handler_->ShowDialog(
          window_helper->GetWindowAndroid()->GetJavaObject().obj());

      // Register to receive a callback to OnAutofillDataAvailable().
      SetModel(manager);
      NotifyAuthNeeded();
    } else {
      CancelAuth();
      LOG(WARNING) << "HTTP Authentication failed because TabAndroid is "
          "missing";
    }
  }

 protected:
  virtual ~LoginHandlerAndroid() {}

  virtual void CloseDialog() OVERRIDE {}

 private:
  scoped_ptr<ChromeHttpAuthHandler> chrome_http_auth_handler_;
};

// static
LoginHandler* LoginHandler::Create(net::AuthChallengeInfo* auth_info,
                                   net::URLRequest* request) {
  return new LoginHandlerAndroid(auth_info, request);
}
