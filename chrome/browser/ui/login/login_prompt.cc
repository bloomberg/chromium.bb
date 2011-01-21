// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_prompt.h"

#include <vector>

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/synchronization/lock.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/browser/tab_contents/constrained_window.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "net/base/auth.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"

using webkit_glue::PasswordForm;

class LoginHandlerImpl;

// Helper to remove the ref from an net::URLRequest to the LoginHandler.
// Should only be called from the IO thread, since it accesses an
// net::URLRequest.
void ResetLoginHandlerForRequest(net::URLRequest* request) {
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
// LoginHandler

LoginHandler::LoginHandler(net::AuthChallengeInfo* auth_info,
                           net::URLRequest* request)
    : handled_auth_(false),
      dialog_(NULL),
      auth_info_(auth_info),
      request_(request),
      password_manager_(NULL),
      login_model_(NULL) {
  // This constructor is called on the I/O thread, so we cannot load the nib
  // here. BuildViewForPasswordManager() will be invoked on the UI thread
  // later, so wait with loading the nib until then.
  DCHECK(request_) << "LoginHandler constructed with NULL request";
  DCHECK(auth_info_) << "LoginHandler constructed with NULL auth info";

  AddRef();  // matched by LoginHandler::ReleaseSoon().

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &LoginHandler::AddObservers));

  if (!ResourceDispatcherHost::RenderViewForRequest(
          request_, &render_process_host_id_,  &tab_contents_id_)) {
    NOTREACHED();
  }
}

LoginHandler::~LoginHandler() {
  SetModel(NULL);
}

void LoginHandler::SetPasswordForm(const webkit_glue::PasswordForm& form) {
  password_form_ = form;
}

void LoginHandler::SetPasswordManager(PasswordManager* password_manager) {
  password_manager_ = password_manager;
}

TabContents* LoginHandler::GetTabContentsForLogin() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return tab_util::GetTabContentsByID(render_process_host_id_,
                                      tab_contents_id_);
}

void LoginHandler::SetAuth(const std::wstring& username,
                           const std::wstring& password) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (TestAndSetAuthHandled())
    return;

  // Tell the password manager the credentials were submitted / accepted.
  if (password_manager_) {
    password_form_.username_value = WideToUTF16Hack(username);
    password_form_.password_value = WideToUTF16Hack(password);
    password_manager_->ProvisionallySavePassword(password_form_);
  }

  // Calling NotifyAuthSupplied() directly instead of posting a task
  // allows other LoginHandler instances to queue their
  // CloseContentsDeferred() before ours.  Closing dialogs in the
  // opposite order as they were created avoids races where remaining
  // dialogs in the same tab may be briefly displayed to the user
  // before they are removed.
  NotifyAuthSupplied(username, password);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &LoginHandler::CloseContentsDeferred));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &LoginHandler::SetAuthDeferred, username, password));
}

void LoginHandler::CancelAuth() {
  if (TestAndSetAuthHandled())
    return;

  // Similar to how we deal with notifications above in SetAuth()
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    NotifyAuthCancelled();
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &LoginHandler::NotifyAuthCancelled));
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &LoginHandler::CloseContentsDeferred));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &LoginHandler::CancelAuthDeferred));
}

void LoginHandler::OnRequestCancelled() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO)) <<
      "Why is OnRequestCancelled called from the UI thread?";

  // Reference is no longer valid.
  request_ = NULL;

  // Give up on auth if the request was cancelled.
  CancelAuth();
}

void LoginHandler::AddObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  registrar_.Add(this, NotificationType::AUTH_SUPPLIED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::AUTH_CANCELLED,
                 NotificationService::AllSources());
}

void LoginHandler::RemoveObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  registrar_.Remove(this, NotificationType::AUTH_SUPPLIED,
                    NotificationService::AllSources());
  registrar_.Remove(this, NotificationType::AUTH_CANCELLED,
                    NotificationService::AllSources());

  DCHECK(registrar_.IsEmpty());
}

void LoginHandler::Observe(NotificationType type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(type == NotificationType::AUTH_SUPPLIED ||
         type == NotificationType::AUTH_CANCELLED);

  TabContents* requesting_contents = GetTabContentsForLogin();
  if (!requesting_contents)
    return;

  // Break out early if we aren't interested in the notification.
  if (WasAuthHandled())
    return;

  LoginNotificationDetails* login_details =
      Details<LoginNotificationDetails>(details).ptr();

  // WasAuthHandled() should always test positive before we publish
  // AUTH_SUPPLIED or AUTH_CANCELLED notifications.
  DCHECK(login_details->handler() != this);

  // Only handle notification for the identical auth info.
  if (*login_details->handler()->auth_info() != *auth_info())
    return;

  // Set or cancel the auth in this handler.
  if (type == NotificationType::AUTH_SUPPLIED) {
    AuthSuppliedLoginNotificationDetails* supplied_details =
        Details<AuthSuppliedLoginNotificationDetails>(details).ptr();
    SetAuth(supplied_details->username(), supplied_details->password());
  } else {
    DCHECK(type == NotificationType::AUTH_CANCELLED);
    CancelAuth();
  }
}

void LoginHandler::SetModel(LoginModel* model) {
  if (login_model_)
    login_model_->SetObserver(NULL);
  login_model_ = model;
  if (login_model_)
    login_model_->SetObserver(this);
}

void LoginHandler::SetDialog(ConstrainedWindow* dialog) {
  dialog_ = dialog;
}

void LoginHandler::NotifyAuthNeeded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (WasAuthHandled())
    return;

  TabContents* requesting_contents = GetTabContentsForLogin();
  if (!requesting_contents)
    return;

  NotificationService* service = NotificationService::current();
  NavigationController* controller = &requesting_contents->controller();
  LoginNotificationDetails details(this);

  service->Notify(NotificationType::AUTH_NEEDED,
                  Source<NavigationController>(controller),
                  Details<LoginNotificationDetails>(&details));
}

void LoginHandler::NotifyAuthCancelled() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(WasAuthHandled());

  TabContents* requesting_contents = GetTabContentsForLogin();
  if (!requesting_contents)
    return;

  NotificationService* service = NotificationService::current();
  NavigationController* controller = &requesting_contents->controller();
  LoginNotificationDetails details(this);

  service->Notify(NotificationType::AUTH_CANCELLED,
                  Source<NavigationController>(controller),
                  Details<LoginNotificationDetails>(&details));
}

void LoginHandler::NotifyAuthSupplied(const std::wstring& username,
                                      const std::wstring& password) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(WasAuthHandled());

  TabContents* requesting_contents = GetTabContentsForLogin();
  if (!requesting_contents)
    return;

  NotificationService* service = NotificationService::current();
  NavigationController* controller = &requesting_contents->controller();
  AuthSuppliedLoginNotificationDetails details(this, username, password);

  service->Notify(NotificationType::AUTH_SUPPLIED,
                  Source<NavigationController>(controller),
                  Details<AuthSuppliedLoginNotificationDetails>(&details));
}

void LoginHandler::ReleaseSoon() {
  if (!TestAndSetAuthHandled()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this, &LoginHandler::CancelAuthDeferred));
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &LoginHandler::NotifyAuthCancelled));
  }

  BrowserThread::PostTask(
    BrowserThread::UI, FROM_HERE,
    NewRunnableMethod(this, &LoginHandler::RemoveObservers));

  // Delete this object once all InvokeLaters have been called.
  BrowserThread::ReleaseSoon(BrowserThread::IO, FROM_HERE, this);
}

// Returns whether authentication had been handled (SetAuth or CancelAuth).
bool LoginHandler::WasAuthHandled() const {
  base::AutoLock lock(handled_auth_lock_);
  bool was_handled = handled_auth_;
  return was_handled;
}

// Marks authentication as handled and returns the previous handled state.
bool LoginHandler::TestAndSetAuthHandled() {
  base::AutoLock lock(handled_auth_lock_);
  bool was_handled = handled_auth_;
  handled_auth_ = true;
  return was_handled;
}

// Calls SetAuth from the IO loop.
void LoginHandler::SetAuthDeferred(const std::wstring& username,
                                   const std::wstring& password) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (request_) {
    request_->SetAuth(WideToUTF16Hack(username), WideToUTF16Hack(password));
    ResetLoginHandlerForRequest(request_);
  }
}

// Calls CancelAuth from the IO loop.
void LoginHandler::CancelAuthDeferred() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (request_) {
    request_->CancelAuth();
    // Verify that CancelAuth doesn't destroy the request via our delegate.
    DCHECK(request_ != NULL);
    ResetLoginHandlerForRequest(request_);
  }
}

// Closes the view_contents from the UI loop.
void LoginHandler::CloseContentsDeferred() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The hosting ConstrainedWindow may have been freed.
  if (dialog_)
    dialog_->CloseConstrainedWindow();
}

// ----------------------------------------------------------------------------
// LoginDialogTask

// This task is run on the UI thread and creates a constrained window with
// a LoginView to prompt the user.  The response will be sent to LoginHandler,
// which then routes it to the net::URLRequest on the I/O thread.
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
    if (!parent_contents || handler_->WasAuthHandled()) {
      // The request may have been cancelled, or it may be for a renderer
      // not hosted by a tab (e.g. an extension). Cancel just in case
      // (cancelling twice is a no-op).
      handler_->CancelAuth();
      return;
    }

    // Tell the password manager to look for saved passwords.
    TabContentsWrapper** wrapper =
        TabContentsWrapper::property_accessor()->GetProperty(
            parent_contents->property_bag());
    if (!wrapper)
      return;
    PasswordManager* password_manager = (*wrapper)->GetPasswordManager();
    std::vector<PasswordForm> v;
    MakeInputForPasswordManager(&v);
    password_manager->OnPasswordFormsFound(v);
    handler_->SetPasswordManager(password_manager);

    string16 host_and_port_hack16 = WideToUTF16Hack(auth_info_->host_and_port);
    string16 realm_hack16 = WideToUTF16Hack(auth_info_->realm);
    string16 explanation = realm_hack16.empty() ?
        l10n_util::GetStringFUTF16(IDS_LOGIN_DIALOG_DESCRIPTION_NO_REALM,
                                   host_and_port_hack16) :
        l10n_util::GetStringFUTF16(IDS_LOGIN_DIALOG_DESCRIPTION,
                                   host_and_port_hack16,
                                   realm_hack16);
    handler_->BuildViewForPasswordManager(password_manager,
                                          UTF16ToWideHack(explanation));
  }

 private:
  // Helper to create a PasswordForm and stuff it into a vector as input
  // for PasswordManager::PasswordFormsFound, the hook into PasswordManager.
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
    if (auth_info_->is_proxy) {
      std::string origin = host_and_port;
      // We don't expect this to already start with http:// or https://.
      DCHECK(origin.find("http://") != 0 && origin.find("https://") != 0);
      origin = std::string("http://") + origin;
      dialog_form.origin = GURL(origin);
    } else if (net::GetHostAndPort(request_url_) != host_and_port) {
      dialog_form.origin = GURL();
      NOTREACHED();  // crbug.com/32718
    } else {
      dialog_form.origin = GURL(request_url_.scheme() + "://" + host_and_port);
    }
    dialog_form.signon_realm = GetSignonRealm(dialog_form.origin, *auth_info_);
    password_manager_input->push_back(dialog_form);
    // Set the password form for the handler (by copy).
    handler_->SetPasswordForm(dialog_form);
  }

  // The url from the net::URLRequest initiating the auth challenge.
  GURL request_url_;

  // Info about who/where/what is asking for authentication.
  scoped_refptr<net::AuthChallengeInfo> auth_info_;

  // Where to send the authentication when obtained.
  // This is owned by the ResourceDispatcherHost that invoked us.
  LoginHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(LoginDialogTask);
};

// ----------------------------------------------------------------------------
// Public API

LoginHandler* CreateLoginPrompt(net::AuthChallengeInfo* auth_info,
                                net::URLRequest* request) {
  LoginHandler* handler = LoginHandler::Create(auth_info, request);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, new LoginDialogTask(
          request->url(), auth_info, handler));
  return handler;
}
