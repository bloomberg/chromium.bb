// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_prompt.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/synchronization/lock.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "net/base/auth.h"
#include "net/base/net_util.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/text_elider.h"

using content::BrowserThread;
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

  info->set_login_delegate(NULL);
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
    signon_realm = auth_info.challenger.ToString();
    signon_realm.append("/");
  } else {
    // Take scheme, host, and port from the url.
    signon_realm = url.GetOrigin().spec();
    // This ends with a "/".
  }
  signon_realm.append(auth_info.realm);
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
      http_network_session_(
          request_->context()->http_transaction_factory()->GetSession()),
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
      base::Bind(&LoginHandler::AddObservers, this));

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

RenderViewHostDelegate* LoginHandler::GetRenderViewHostDelegate() const {
  RenderViewHost* rvh = RenderViewHost::FromID(render_process_host_id_,
                                               tab_contents_id_);
  if (!rvh)
    return NULL;

  return rvh->delegate();
}

void LoginHandler::SetAuth(const string16& username,
                           const string16& password) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (TestAndSetAuthHandled())
    return;

  // Tell the password manager the credentials were submitted / accepted.
  if (password_manager_) {
    password_form_.username_value = username;
    password_form_.password_value = password;
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
      base::Bind(&LoginHandler::CloseContentsDeferred, this));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&LoginHandler::SetAuthDeferred, this, username, password));
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
        base::Bind(&LoginHandler::NotifyAuthCancelled, this));
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&LoginHandler::CloseContentsDeferred, this));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&LoginHandler::CancelAuthDeferred, this));
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

  // This is probably OK; we need to listen to everything and we break out of
  // the Observe() if we aren't handling the same auth_info().
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_CANCELLED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void LoginHandler::RemoveObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  registrar_.Remove(
      this, chrome::NOTIFICATION_AUTH_SUPPLIED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(
      this, chrome::NOTIFICATION_AUTH_CANCELLED,
      content::NotificationService::AllBrowserContextsAndSources());

  DCHECK(registrar_.IsEmpty());
}

void LoginHandler::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(type == chrome::NOTIFICATION_AUTH_SUPPLIED ||
         type == chrome::NOTIFICATION_AUTH_CANCELLED);

  TabContents* requesting_contents = GetTabContentsForLogin();
  if (!requesting_contents)
    return;

  // Break out early if we aren't interested in the notification.
  if (WasAuthHandled())
    return;

  LoginNotificationDetails* login_details =
      content::Details<LoginNotificationDetails>(details).ptr();

  // WasAuthHandled() should always test positive before we publish
  // AUTH_SUPPLIED or AUTH_CANCELLED notifications.
  DCHECK(login_details->handler() != this);

  // Only handle notification for the identical auth info.
  if (!login_details->handler()->auth_info()->Equals(*auth_info()))
    return;

  // Ignore login notification events from other profiles.
  if (login_details->handler()->http_network_session_ !=
      http_network_session_)
    return;

  // Set or cancel the auth in this handler.
  if (type == chrome::NOTIFICATION_AUTH_SUPPLIED) {
    AuthSuppliedLoginNotificationDetails* supplied_details =
        content::Details<AuthSuppliedLoginNotificationDetails>(details).ptr();
    SetAuth(supplied_details->username(), supplied_details->password());
  } else {
    DCHECK(type == chrome::NOTIFICATION_AUTH_CANCELLED);
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

  content::NotificationService* service =
      content::NotificationService::current();
  NavigationController* controller = NULL;

  TabContents* requesting_contents = GetTabContentsForLogin();
  if (requesting_contents)
    controller = &requesting_contents->controller();

  LoginNotificationDetails details(this);

  service->Notify(chrome::NOTIFICATION_AUTH_NEEDED,
                  content::Source<NavigationController>(controller),
                  content::Details<LoginNotificationDetails>(&details));
}

void LoginHandler::NotifyAuthCancelled() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(WasAuthHandled());

  content::NotificationService* service =
      content::NotificationService::current();
  NavigationController* controller = NULL;

  TabContents* requesting_contents = GetTabContentsForLogin();
  if (requesting_contents)
    controller = &requesting_contents->controller();

  LoginNotificationDetails details(this);

  service->Notify(chrome::NOTIFICATION_AUTH_CANCELLED,
                  content::Source<NavigationController>(controller),
                  content::Details<LoginNotificationDetails>(&details));
}

void LoginHandler::NotifyAuthSupplied(const string16& username,
                                      const string16& password) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(WasAuthHandled());

  TabContents* requesting_contents = GetTabContentsForLogin();
  if (!requesting_contents)
    return;

  content::NotificationService* service =
      content::NotificationService::current();
  NavigationController* controller = &requesting_contents->controller();
  AuthSuppliedLoginNotificationDetails details(this, username, password);

  service->Notify(
      chrome::NOTIFICATION_AUTH_SUPPLIED,
      content::Source<NavigationController>(controller),
      content::Details<AuthSuppliedLoginNotificationDetails>(&details));
}

void LoginHandler::ReleaseSoon() {
  if (!TestAndSetAuthHandled()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&LoginHandler::CancelAuthDeferred, this));
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&LoginHandler::NotifyAuthCancelled, this));
  }

  BrowserThread::PostTask(
    BrowserThread::UI, FROM_HERE,
    base::Bind(&LoginHandler::RemoveObservers, this));

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
void LoginHandler::SetAuthDeferred(const string16& username,
                                   const string16& password) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (request_) {
    request_->SetAuth(net::AuthCredentials(username, password));
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

// Helper to create a PasswordForm and stuff it into a vector as input
// for PasswordManager::PasswordFormsFound, the hook into PasswordManager.
void MakeInputForPasswordManager(
    const GURL& request_url,
    net::AuthChallengeInfo* auth_info,
    LoginHandler* handler,
    std::vector<PasswordForm>* password_manager_input) {
  PasswordForm dialog_form;
  if (LowerCaseEqualsASCII(auth_info->scheme, "basic")) {
    dialog_form.scheme = PasswordForm::SCHEME_BASIC;
  } else if (LowerCaseEqualsASCII(auth_info->scheme, "digest")) {
    dialog_form.scheme = PasswordForm::SCHEME_DIGEST;
  } else {
    dialog_form.scheme = PasswordForm::SCHEME_OTHER;
  }
  std::string host_and_port(auth_info->challenger.ToString());
  if (auth_info->is_proxy) {
    std::string origin = host_and_port;
    // We don't expect this to already start with http:// or https://.
    DCHECK(origin.find("http://") != 0 && origin.find("https://") != 0);
    origin = std::string("http://") + origin;
    dialog_form.origin = GURL(origin);
  } else if (!auth_info->challenger.Equals(
      net::HostPortPair::FromURL(request_url))) {
    dialog_form.origin = GURL();
    NOTREACHED();  // crbug.com/32718
  } else {
    dialog_form.origin = GURL(request_url.scheme() + "://" + host_and_port);
  }
  dialog_form.signon_realm = GetSignonRealm(dialog_form.origin, *auth_info);
  password_manager_input->push_back(dialog_form);
  // Set the password form for the handler (by copy).
  handler->SetPasswordForm(dialog_form);
}

// This callback is run on the UI thread and creates a constrained window with
// a LoginView to prompt the user.  The response will be sent to LoginHandler,
// which then routes it to the net::URLRequest on the I/O thread.
void LoginDialogCallback(const GURL& request_url,
                         net::AuthChallengeInfo* auth_info,
                         LoginHandler* handler) {
  TabContents* parent_contents = handler->GetTabContentsForLogin();
  if (!parent_contents || handler->WasAuthHandled()) {
    // The request may have been cancelled, or it may be for a renderer
    // not hosted by a tab (e.g. an extension). Cancel just in case
    // (cancelling twice is a no-op).
    handler->CancelAuth();
    return;
  }

  // Tell the password manager to look for saved passwords.
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(parent_contents);
  if (!wrapper)
    NOTREACHED() << "Login dialog created for TabContents with no wrapper";

  PasswordManager* password_manager = wrapper->password_manager();
  std::vector<PasswordForm> v;
  MakeInputForPasswordManager(request_url, auth_info, handler, &v);
  password_manager->OnPasswordFormsFound(v);
  handler->SetPasswordManager(password_manager);

  // The realm is controlled by the remote server, so there is no reason
  // to believe it is of a reasonable length.
  string16 elided_realm;
  ui::ElideString(UTF8ToUTF16(auth_info->realm), 120, &elided_realm);

  string16 host_and_port = ASCIIToUTF16(auth_info->challenger.ToString());
  string16 explanation = elided_realm.empty() ?
      l10n_util::GetStringFUTF16(IDS_LOGIN_DIALOG_DESCRIPTION_NO_REALM,
                                 host_and_port) :
      l10n_util::GetStringFUTF16(IDS_LOGIN_DIALOG_DESCRIPTION,
                                 host_and_port,
                                 elided_realm);
  handler->BuildViewForPasswordManager(password_manager, explanation);
}

// ----------------------------------------------------------------------------
// Public API

LoginHandler* CreateLoginPrompt(net::AuthChallengeInfo* auth_info,
                                net::URLRequest* request) {
  LoginHandler* handler = LoginHandler::Create(auth_info, request);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&LoginDialogCallback, request->url(),
                 make_scoped_refptr(auth_info), make_scoped_refptr(handler)));
  return handler;
}
