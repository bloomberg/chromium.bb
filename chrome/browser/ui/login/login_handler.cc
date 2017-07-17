// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_handler.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/login/login_interstitial_delegate.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/origin_util.h"
#include "extensions/features/features.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_base.h"
#include "extensions/browser/view_type_utils.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/blocked_content/app_modal_dialog_helper.h"
#endif

using autofill::PasswordForm;
using content::BrowserThread;
using content::NavigationController;
using content::ResourceDispatcherHost;
using content::ResourceRequestInfo;
using content::WebContents;

class LoginHandlerImpl;

namespace {

// Auth prompt types for UMA. Do not reorder or delete entries; only add to the
// end.
enum AuthPromptType {
  AUTH_PROMPT_TYPE_WITH_INTERSTITIAL = 0,
  AUTH_PROMPT_TYPE_MAIN_FRAME = 1,
  AUTH_PROMPT_TYPE_SUBRESOURCE_SAME_ORIGIN = 2,
  AUTH_PROMPT_TYPE_SUBRESOURCE_CROSS_ORIGIN = 3,
  AUTH_PROMPT_TYPE_ENUM_COUNT = 4
};

// Helper to remove the ref from an net::URLRequest to the LoginHandler.
// Should only be called from the IO thread, since it accesses an
// net::URLRequest.
void ResetLoginHandlerForRequest(net::URLRequest* request) {
  ResourceDispatcherHost::Get()->ClearLoginDelegateForRequest(request);
}

void RecordHttpAuthPromptType(AuthPromptType prompt_type) {
  UMA_HISTOGRAM_ENUMERATION("Net.HttpAuthPromptType", prompt_type,
                            AUTH_PROMPT_TYPE_ENUM_COUNT);
}

}  // namespace

// ----------------------------------------------------------------------------
// LoginHandler

LoginHandler::LoginModelData::LoginModelData(
    password_manager::LoginModel* login_model,
    const autofill::PasswordForm& observed_form)
    : model(login_model), form(observed_form) {
  DCHECK(model);
}

LoginHandler::LoginHandler(net::AuthChallengeInfo* auth_info,
                           net::URLRequest* request)
    : handled_auth_(false),
      auth_info_(auth_info),
      request_(request),
      http_network_session_(
          request_->context()->http_transaction_factory()->GetSession()),
      password_manager_(NULL),
      login_model_(NULL) {
  // This constructor is called on the I/O thread, so we cannot load the nib
  // here. BuildViewImpl() will be invoked on the UI thread later, so wait with
  // loading the nib until then.
  DCHECK(request_) << "LoginHandler constructed with NULL request";
  DCHECK(auth_info_.get()) << "LoginHandler constructed with NULL auth info";

  AddRef();  // matched by LoginHandler::ReleaseSoon().

  const content::ResourceRequestInfo* info =
      ResourceRequestInfo::ForRequest(request);
  DCHECK(info);
  web_contents_getter_ = info->GetWebContentsGetterForRequest();

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(&LoginHandler::AddObservers, this));
}

void LoginHandler::OnRequestCancelled() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO)) <<
      "Why is OnRequestCancelled called from the UI thread?";

  // Reference is no longer valid.
  request_ = NULL;

  // Give up on auth if the request was cancelled. Since the dialog was canceled
  // by the ResourceLoader and not the user, we should cancel the navigation as
  // well. This can happen when a new navigation interrupts the current one.
  DoCancelAuth(true);
}

void LoginHandler::BuildViewWithPasswordManager(
    const base::string16& authority,
    const base::string16& explanation,
    password_manager::PasswordManager* password_manager,
    const autofill::PasswordForm& observed_form) {
  password_manager_ = password_manager;
  password_form_ = observed_form;
  LoginHandler::LoginModelData model_data(password_manager, observed_form);
  BuildViewImpl(authority, explanation, &model_data);
}

void LoginHandler::BuildViewWithoutPasswordManager(
    const base::string16& authority,
    const base::string16& explanation) {
  BuildViewImpl(authority, explanation, nullptr);
}

WebContents* LoginHandler::GetWebContentsForLogin() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return web_contents_getter_.Run();
}

password_manager::PasswordManager* LoginHandler::GetPasswordManagerForLogin() {
  password_manager::PasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(GetWebContentsForLogin());
  return client ? client->GetPasswordManager() : nullptr;
}

void LoginHandler::SetAuth(const base::string16& username,
                           const base::string16& password) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<password_manager::BrowserSavePasswordProgressLogger> logger;
  if (password_manager_ &&
      password_manager_->client()->GetLogManager()->IsLoggingActive()) {
    logger.reset(new password_manager::BrowserSavePasswordProgressLogger(
        password_manager_->client()->GetLogManager()));
    logger->LogMessage(
        autofill::SavePasswordProgressLogger::STRING_SET_AUTH_METHOD);
  }

  bool already_handled = TestAndSetAuthHandled();
  if (logger) {
    logger->LogBoolean(
        autofill::SavePasswordProgressLogger::STRING_AUTHENTICATION_HANDLED,
        already_handled);
  }
  if (already_handled)
    return;

  // Tell the password manager the credentials were submitted / accepted.
  if (password_manager_) {
    password_form_.username_value = username;
    password_form_.password_value = password;
    password_manager_->ProvisionallySavePassword(password_form_, nullptr);
    if (logger) {
      logger->LogPasswordForm(
          autofill::SavePasswordProgressLogger::STRING_LOGINHANDLER_FORM,
          password_form_);
    }
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
      base::BindOnce(&LoginHandler::CloseContentsDeferred, this));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&LoginHandler::SetAuthDeferred, this, username, password));
}

void LoginHandler::CancelAuth() {
  // Cancel the auth without canceling the navigation, so that the auth error
  // page commits.
  DoCancelAuth(false);
}

void LoginHandler::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(type == chrome::NOTIFICATION_AUTH_SUPPLIED ||
         type == chrome::NOTIFICATION_AUTH_CANCELLED);

  WebContents* requesting_contents = GetWebContentsForLogin();
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

// Returns whether authentication had been handled (SetAuth or CancelAuth).
bool LoginHandler::WasAuthHandled() const {
  base::AutoLock lock(handled_auth_lock_);
  bool was_handled = handled_auth_;
  return was_handled;
}

LoginHandler::~LoginHandler() {
  ResetModel();
}

void LoginHandler::SetModel(LoginModelData model_data) {
  ResetModel();
  login_model_ = model_data.model;
  login_model_->AddObserverAndDeliverCredentials(this, model_data.form);
}

void LoginHandler::ResetModel() {
  if (login_model_)
    login_model_->RemoveObserver(this);
  login_model_ = nullptr;
}

void LoginHandler::NotifyAuthNeeded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (WasAuthHandled())
    return;

  content::NotificationService* service =
      content::NotificationService::current();
  NavigationController* controller = NULL;

  WebContents* requesting_contents = GetWebContentsForLogin();
  if (requesting_contents)
    controller = &requesting_contents->GetController();

  LoginNotificationDetails details(this);

  service->Notify(chrome::NOTIFICATION_AUTH_NEEDED,
                  content::Source<NavigationController>(controller),
                  content::Details<LoginNotificationDetails>(&details));
}

void LoginHandler::ReleaseSoon() {
  if (!TestAndSetAuthHandled()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&LoginHandler::CancelAuthDeferred, this));
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&LoginHandler::NotifyAuthCancelled, this, false));
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(&LoginHandler::RemoveObservers, this));

  // Delete this object once all InvokeLaters have been called.
  BrowserThread::ReleaseSoon(BrowserThread::IO, FROM_HERE, this);
}

void LoginHandler::AddObservers() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // This is probably OK; we need to listen to everything and we break out of
  // the Observe() if we aren't handling the same auth_info().
  registrar_.reset(new content::NotificationRegistrar);
  registrar_->Add(this, chrome::NOTIFICATION_AUTH_SUPPLIED,
                  content::NotificationService::AllBrowserContextsAndSources());
  registrar_->Add(this, chrome::NOTIFICATION_AUTH_CANCELLED,
                  content::NotificationService::AllBrowserContextsAndSources());

#if !defined(OS_ANDROID)
  WebContents* requesting_contents = GetWebContentsForLogin();
  if (requesting_contents)
    dialog_helper_.reset(new AppModalDialogHelper(requesting_contents));
#endif
}

void LoginHandler::RemoveObservers() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  registrar_.reset();
}

void LoginHandler::NotifyAuthSupplied(const base::string16& username,
                                      const base::string16& password) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(WasAuthHandled());

  WebContents* requesting_contents = GetWebContentsForLogin();
  if (!requesting_contents)
    return;

  content::NotificationService* service =
      content::NotificationService::current();
  NavigationController* controller =
      &requesting_contents->GetController();
  AuthSuppliedLoginNotificationDetails details(this, username, password);

  service->Notify(
      chrome::NOTIFICATION_AUTH_SUPPLIED,
      content::Source<NavigationController>(controller),
      content::Details<AuthSuppliedLoginNotificationDetails>(&details));
}

void LoginHandler::NotifyAuthCancelled(bool dismiss_navigation) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(WasAuthHandled());

  content::NotificationService* service =
      content::NotificationService::current();
  NavigationController* controller = NULL;

  WebContents* requesting_contents = GetWebContentsForLogin();
  if (requesting_contents)
    controller = &requesting_contents->GetController();

  if (dismiss_navigation && interstitial_delegate_)
    interstitial_delegate_->DontProceed();

  LoginNotificationDetails details(this);
  service->Notify(chrome::NOTIFICATION_AUTH_CANCELLED,
                  content::Source<NavigationController>(controller),
                  content::Details<LoginNotificationDetails>(&details));
}

// Marks authentication as handled and returns the previous handled state.
bool LoginHandler::TestAndSetAuthHandled() {
  base::AutoLock lock(handled_auth_lock_);
  bool was_handled = handled_auth_;
  handled_auth_ = true;
  return was_handled;
}

// Calls SetAuth from the IO loop.
void LoginHandler::SetAuthDeferred(const base::string16& username,
                                   const base::string16& password) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (request_) {
    request_->SetAuth(net::AuthCredentials(username, password));
    ResetLoginHandlerForRequest(request_);
  }
}

void LoginHandler::DoCancelAuth(bool dismiss_navigation) {
  if (TestAndSetAuthHandled())
    return;

  // Similar to how we deal with notifications above in SetAuth().
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    NotifyAuthCancelled(dismiss_navigation);
  } else {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(&LoginHandler::NotifyAuthCancelled,
                                           this, dismiss_navigation));
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&LoginHandler::CloseContentsDeferred, this));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&LoginHandler::CancelAuthDeferred, this));
}

// Calls CancelAuth from the IO loop.
void LoginHandler::CancelAuthDeferred() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (request_) {
    request_->CancelAuth();
    // Verify that CancelAuth doesn't destroy the request via our delegate.
    DCHECK(request_ != NULL);
    ResetLoginHandlerForRequest(request_);
  }
}

// Closes the view_contents from the UI loop.
void LoginHandler::CloseContentsDeferred() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CloseDialog();
  if (interstitial_delegate_)
    interstitial_delegate_->Proceed();
#if !defined(OS_ANDROID)
  dialog_helper_.reset();
#endif
}

// static
std::string LoginHandler::GetSignonRealm(
    const GURL& url,
    const net::AuthChallengeInfo& auth_info) {
  std::string signon_realm;
  if (auth_info.is_proxy) {
    // Historically we've been storing the signon realm for proxies using
    // net::HostPortPair::ToString().
    net::HostPortPair host_port_pair =
        net::HostPortPair::FromURL(auth_info.challenger.GetURL());
    signon_realm = host_port_pair.ToString();
    signon_realm.append("/");
  } else {
    // Take scheme, host, and port from the url.
    signon_realm = url.GetOrigin().spec();
    // This ends with a "/".
  }
  signon_realm.append(auth_info.realm);
  return signon_realm;
}

// static
PasswordForm LoginHandler::MakeInputForPasswordManager(
    const GURL& request_url,
    const net::AuthChallengeInfo& auth_info) {
  PasswordForm dialog_form;
  if (base::LowerCaseEqualsASCII(auth_info.scheme, net::kBasicAuthScheme)) {
    dialog_form.scheme = PasswordForm::SCHEME_BASIC;
  } else if (base::LowerCaseEqualsASCII(auth_info.scheme,
                                        net::kDigestAuthScheme)) {
    dialog_form.scheme = PasswordForm::SCHEME_DIGEST;
  } else {
    dialog_form.scheme = PasswordForm::SCHEME_OTHER;
  }
  dialog_form.origin = auth_info.challenger.GetURL();
  DCHECK(auth_info.is_proxy ||
         auth_info.challenger.IsSameOriginWith(url::Origin(request_url)));
  dialog_form.signon_realm = GetSignonRealm(dialog_form.origin, auth_info);
  return dialog_form;
}

// static
void LoginHandler::GetDialogStrings(const GURL& request_url,
                                    const net::AuthChallengeInfo& auth_info,
                                    base::string16* authority,
                                    base::string16* explanation) {
  GURL authority_url;

  if (auth_info.is_proxy) {
    *authority = l10n_util::GetStringFUTF16(
        IDS_LOGIN_DIALOG_PROXY_AUTHORITY,
        url_formatter::FormatOriginForSecurityDisplay(
            auth_info.challenger, url_formatter::SchemeDisplay::SHOW));
    authority_url = auth_info.challenger.GetURL();
  } else {
    *authority = url_formatter::FormatUrlForSecurityDisplay(request_url);
#if defined(OS_ANDROID)
    // Android concatenates with a space rather than displaying on two separate
    // lines, so it needs some surrounding text.
    *authority =
        l10n_util::GetStringFUTF16(IDS_LOGIN_DIALOG_AUTHORITY, *authority);
#endif
    authority_url = request_url;
  }

  if (!content::IsOriginSecure(authority_url)) {
    // TODO(asanka): The string should be different for proxies and servers.
    // http://crbug.com/620756
    *explanation = l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_NOT_PRIVATE);
  } else {
    explanation->clear();
  }
}

// static
void LoginHandler::ShowLoginPrompt(const GURL& request_url,
                                   net::AuthChallengeInfo* auth_info,
                                   LoginHandler* handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebContents* parent_contents = handler->GetWebContentsForLogin();
  if (!parent_contents)
    return;
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(parent_contents);
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_AUTH_NEEDED);
    return;
  }

  base::string16 authority;
  base::string16 explanation;
  GetDialogStrings(request_url, *auth_info, &authority, &explanation);

  password_manager::PasswordManager* password_manager =
      handler->GetPasswordManagerForLogin();

  if (!password_manager) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    // A WebContents in a <webview> (a GuestView type) does not have a password
    // manager, but still needs to be able to show login prompts.
    const auto* guest =
        guest_view::GuestViewBase::FromWebContents(parent_contents);
    if (guest &&
        extensions::GetViewType(guest->owner_web_contents()) !=
            extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
      handler->BuildViewWithoutPasswordManager(authority, explanation);
      return;
    }
#endif
    handler->CancelAuth();
    return;
  }

  if (password_manager &&
      password_manager->client()->GetLogManager()->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(
        password_manager->client()->GetLogManager());
    logger.LogMessage(
        autofill::SavePasswordProgressLogger::STRING_SHOW_LOGIN_PROMPT_METHOD);
  }

  PasswordForm observed_form(
      LoginHandler::MakeInputForPasswordManager(request_url, *auth_info));
  handler->BuildViewWithPasswordManager(authority, explanation,
                                        password_manager, observed_form);
}

// static
void LoginHandler::LoginDialogCallback(const GURL& request_url,
                                       net::AuthChallengeInfo* auth_info,
                                       LoginHandler* handler,
                                       bool is_main_frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebContents* parent_contents = handler->GetWebContentsForLogin();
  if (!parent_contents || handler->WasAuthHandled()) {
    // The request may have been canceled, or it may be for a renderer not
    // hosted by a tab (e.g. an extension). Cancel just in case (canceling twice
    // is a no-op).
    handler->CancelAuth();
    return;
  }

  // Check if this is a main frame navigation and
  // (a) if the request is cross origin or
  // (b) if an interstitial is already being shown or
  // (c) the prompt is for proxy authentication
  // (d) we're not displaying a standalone app
  //
  // For (a), there are two different ways the navigation can occur:
  // 1- The user enters the resource URL in the omnibox.
  // 2- The page redirects to the resource.
  // In both cases, the last committed URL is different than the resource URL,
  // so checking it is sufficient.
  // Note that (1) will not be true once site isolation is enabled, as any
  // navigation could cause a cross-process swap, including link clicks.
  //
  // For (b), the login interstitial should always replace an existing
  // interstitial. This is because |LoginHandler::CloseContentsDeferred| tries
  // to proceed whatever interstitial is being shown when the login dialog is
  // closed, so that interstitial should only be a login interstitial.
  //
  // For (c), the authority information in the omnibox will be (and should be)
  // different from the authority information in the authentication prompt. An
  // interstitial with an empty URL clears the omnibox and reduces the possible
  // user confusion that may result from the different authority information
  // being displayed simultaneously. This is specially important when the proxy
  // is accessed via an open connection while the target server is considered
  // secure.
  const bool is_cross_origin_request =
      parent_contents->GetLastCommittedURL().GetOrigin() !=
      request_url.GetOrigin();
  if (is_main_frame &&
      (is_cross_origin_request || parent_contents->ShowingInterstitialPage() ||
       auth_info->is_proxy) &&
      parent_contents->GetDelegate()->GetDisplayMode(parent_contents) !=
          blink::kWebDisplayModeStandalone) {
    RecordHttpAuthPromptType(AUTH_PROMPT_TYPE_WITH_INTERSTITIAL);

    // Show a blank interstitial for main-frame, cross origin requests
    // so that the correct URL is shown in the omnibox.
    base::Closure callback =
        base::Bind(&LoginHandler::ShowLoginPrompt, request_url,
                   base::RetainedRef(auth_info), base::RetainedRef(handler));
    // The interstitial delegate is owned by the interstitial that it creates.
    // This cancels any existing interstitial.
    handler->SetInterstitialDelegate(
        (new LoginInterstitialDelegate(
             parent_contents, auth_info->is_proxy ? GURL() : request_url,
             callback))
            ->GetWeakPtr());

  } else {
    if (is_main_frame) {
      RecordHttpAuthPromptType(AUTH_PROMPT_TYPE_MAIN_FRAME);
    } else {
      RecordHttpAuthPromptType(is_cross_origin_request
                                   ? AUTH_PROMPT_TYPE_SUBRESOURCE_CROSS_ORIGIN
                                   : AUTH_PROMPT_TYPE_SUBRESOURCE_SAME_ORIGIN);
    }
    ShowLoginPrompt(request_url, auth_info, handler);
  }
}

// ----------------------------------------------------------------------------
// Public API

LoginHandler* CreateLoginPrompt(net::AuthChallengeInfo* auth_info,
                                net::URLRequest* request) {
  bool is_main_frame =
      (request->load_flags() & net::LOAD_MAIN_FRAME_DEPRECATED) != 0;
  LoginHandler* handler = LoginHandler::Create(auth_info, request);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&LoginHandler::LoginDialogCallback, request->url(),
                     base::RetainedRef(auth_info), base::RetainedRef(handler),
                     is_main_frame));
  return handler;
}
