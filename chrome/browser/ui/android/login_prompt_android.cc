// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_prompt.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/chrome_http_auth_handler.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/auth.h"

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
      const string16& username,
      const string16& password) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(chrome_http_auth_handler_.get() != NULL);
    chrome_http_auth_handler_->OnAutofillDataAvailable(
        username, password);
  }

  virtual void BuildViewForPasswordManager(
      PasswordManager* manager,
      const string16& explanation) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    // Get pointer to TabAndroid
    content::WebContents* web_contents = GetWebContentsForLogin();
    CHECK(web_contents);
    TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);

    // Notify TabAndroid that HTTP authentication is required for current page.
    if (tab_android) {
      chrome_http_auth_handler_.reset(new ChromeHttpAuthHandler(explanation));
      chrome_http_auth_handler_->Init();
      chrome_http_auth_handler_->SetObserver(this);

      tab_android->OnReceivedHttpAuthRequest(
          chrome_http_auth_handler_.get()->GetJavaObject(),
          ASCIIToUTF16(auth_info()->challenger.ToString()),
          UTF8ToUTF16(auth_info()->realm));

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
