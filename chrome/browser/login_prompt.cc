// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_prompt.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/lock.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/browser/tab_contents/constrained_window.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "net/base/auth.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"

using webkit_glue::PasswordForm;

class LoginHandlerImpl;

// Helper to remove the ref from an URLRequest to the LoginHandler.
// Should only be called from the IO thread, since it accesses an URLRequest.
void ResetLoginHandlerForRequest(URLRequest* request) {
  ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  if (!info)
    return;

  info->set_login_handler(NULL);
}

// Get the signon_realm under which this auth info should be stored.
//
// The format of the signon_realm for proxy auth is:
//     proxy-host/auth-realm
// The format of the signon_realm for server auth is:
//     url-scheme://url-host[:url-port]/auth-realm
//
// Be careful when changing this function, since you could make existing
// saved logins un-retrievable.
std::string GetSignonRealm(const GURL& url,
                           const net::AuthChallengeInfo& auth_info) {
  std::string signon_realm;
  if (auth_info.is_proxy) {
    signon_realm = WideToASCII(auth_info.host_and_port);
    signon_realm.append("/");
  } else {
    // Take scheme, host, and port from the url.
    signon_realm = url.GetOrigin().spec();
    // This ends with a "/".
  }
  signon_realm.append(WideToUTF8(auth_info.realm));
  return signon_realm;
}

// ----------------------------------------------------------------------------
// LoginDialogTask

// This task is run on the UI thread and creates a constrained window with
// a LoginView to prompt the user.  The response will be sent to LoginHandler,
// which then routes it to the URLRequest on the I/O thread.
class LoginDialogTask : public Task {
 public:
  LoginDialogTask(const GURL& request_url,
                  net::AuthChallengeInfo* auth_info,
                  LoginHandler* handler)
      : request_url_(request_url), auth_info_(auth_info), handler_(handler) {
  }
  virtual ~LoginDialogTask() {
  }

  void Run() {
    TabContents* parent_contents = handler_->GetTabContentsForLogin();
    if (!parent_contents) {
      // The request was probably cancelled.
      return;
    }

    // Tell the password manager to look for saved passwords.
    PasswordManager* password_manager =
        parent_contents->GetPasswordManager();
    std::vector<PasswordForm> v;
    MakeInputForPasswordManager(&v);
    password_manager->PasswordFormsSeen(v);
    handler_->SetPasswordManager(password_manager);

    std::wstring explanation = auth_info_->realm.empty() ?
        l10n_util::GetStringF(IDS_LOGIN_DIALOG_DESCRIPTION_NO_REALM,
                              auth_info_->host_and_port) :
        l10n_util::GetStringF(IDS_LOGIN_DIALOG_DESCRIPTION,
                              auth_info_->host_and_port,
                              auth_info_->realm);
    handler_->BuildViewForPasswordManager(password_manager,
                                          explanation);
  }

 private:
  // Helper to create a PasswordForm and stuff it into a vector as input
  // for PasswordManager::PasswordFormsSeen, the hook into PasswordManager.
  void MakeInputForPasswordManager(
      std::vector<PasswordForm>* password_manager_input) {
    PasswordForm dialog_form;
    if (LowerCaseEqualsASCII(auth_info_->scheme, "basic")) {
      dialog_form.scheme = PasswordForm::SCHEME_BASIC;
    } else if (LowerCaseEqualsASCII(auth_info_->scheme, "digest")) {
      dialog_form.scheme = PasswordForm::SCHEME_DIGEST;
    } else {
      dialog_form.scheme = PasswordForm::SCHEME_OTHER;
    }
    std::string host_and_port(WideToASCII(auth_info_->host_and_port));
    if (net::GetHostAndPort(request_url_) != host_and_port) {
      dialog_form.origin = GURL();
      NOTREACHED();
    } else if (auth_info_->is_proxy) {
      std::string origin = host_and_port;
      // We don't expect this to already start with http:// or https://.
      DCHECK(origin.find("http://") != 0 && origin.find("https://") != 0);
      origin = std::string("http://") + origin;
      dialog_form.origin = GURL(origin);
    } else {
      dialog_form.origin = GURL(request_url_.scheme() + "://" + host_and_port);
    }
    dialog_form.signon_realm = GetSignonRealm(dialog_form.origin, *auth_info_);
    password_manager_input->push_back(dialog_form);
    // Set the password form for the handler (by copy).
    handler_->SetPasswordForm(dialog_form);
  }

  // The url from the URLRequest initiating the auth challenge.
  GURL request_url_;

  // Info about who/where/what is asking for authentication.
  scoped_refptr<net::AuthChallengeInfo> auth_info_;

  // Where to send the authentication when obtained.
  // This is owned by the ResourceDispatcherHost that invoked us.
  LoginHandler* handler_;

  DISALLOW_EVIL_CONSTRUCTORS(LoginDialogTask);
};

// ----------------------------------------------------------------------------
// Public API

LoginHandler* CreateLoginPrompt(net::AuthChallengeInfo* auth_info,
                                URLRequest* request) {
  LoginHandler* handler = LoginHandler::Create(request);
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE, new LoginDialogTask(
          request->url(), auth_info, handler));
  return handler;
}
