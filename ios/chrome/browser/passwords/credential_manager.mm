// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/credential_manager.h"

#include <utility>

#import "base/ios/weak_nsobject.h"
#include "base/mac/bind_objc_block.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#import "ios/chrome/browser/passwords/js_credential_manager.h"
#import "ios/web/public/url_scheme_util.h"
#include "ios/web/public/web_state/credential.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#include "ios/web/public/web_state/web_state.h"
#include "url/origin.h"

namespace {

// Converts a password_manager::CredentialInfo to a web::Credential.
web::Credential WebCredentialFromCredentialInfo(
    const password_manager::CredentialInfo& credential_info) {
  web::Credential credential;
  switch (credential_info.type) {
    case password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY:
      credential.type = web::CredentialType::CREDENTIAL_TYPE_EMPTY;
      break;
    case password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD:
      credential.type = web::CredentialType::CREDENTIAL_TYPE_PASSWORD;
      break;
    case password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED:
      credential.type = web::CredentialType::CREDENTIAL_TYPE_FEDERATED;
      break;
  }
  credential.id = credential_info.id;
  credential.name = credential_info.name;
  credential.avatar_url = credential_info.icon;
  credential.password = credential_info.password;
  credential.federation_origin = credential_info.federation;
  return credential;
}

// Converts a web::Credential to a password_manager::CredentialInfo.
password_manager::CredentialInfo CredentialInfoFromWebCredential(
    const web::Credential& credential) {
  password_manager::CredentialInfo credential_info;
  switch (credential.type) {
    case web::CredentialType::CREDENTIAL_TYPE_EMPTY:
      credential_info.type =
          password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY;
      break;
    case web::CredentialType::CREDENTIAL_TYPE_PASSWORD:
      credential_info.type =
          password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD;
      break;
    case web::CredentialType::CREDENTIAL_TYPE_FEDERATED:
      credential_info.type =
          password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED;
      break;
  }
  credential_info.id = credential.id;
  credential_info.name = credential.name;
  credential_info.icon = credential.avatar_url;
  credential_info.password = credential.password;
  credential_info.federation = credential.federation_origin;
  return credential_info;
}

}  // namespace

CredentialManager::CredentialManager(
    web::WebState* web_state,
    password_manager::PasswordManagerClient* client,
    password_manager::PasswordManagerDriver* driver,
    JSCredentialManager* js_manager)
    : web::WebStateObserver(web_state),
      pending_request_(nullptr),
      form_manager_(nullptr),
      js_manager_([js_manager retain]),
      client_(client),
      driver_(driver),
      weak_factory_(this) {
  zero_click_sign_in_enabled_.Init(
      password_manager::prefs::kCredentialsEnableAutosignin,
      client_->GetPrefs());
}

CredentialManager::~CredentialManager() = default;

void CredentialManager::PageLoaded(
    web::PageLoadCompletionStatus load_completion_status) {
  // Ensure the JavaScript is loaded when the page finishes loading.
  web::URLVerificationTrustLevel trust_level =
      web::URLVerificationTrustLevel::kNone;
  const GURL page_url(web_state()->GetCurrentURL(&trust_level));
  if (trust_level != web::URLVerificationTrustLevel::kAbsolute ||
      !web::UrlHasWebScheme(page_url) || !web_state()->ContentIsHTML()) {
    return;
  }
  [js_manager_ inject];
}

void CredentialManager::CredentialsRequested(
    int request_id,
    const GURL& source_url,
    bool zero_click_only,
    const std::vector<std::string>& federations,
    bool is_user_initiated) {
  // Invoked when the page invokes navigator.credentials.request(), this
  // function will attempt to retrieve a Credential from the PasswordStore that
  // meets the specified parameters and, if successful, send it back to the page
  // via SendCredentialByID.
  DCHECK_GE(request_id, 0);
  password_manager::PasswordStore* store = GetPasswordStore();

  // If there's an outstanding request, or the PasswordStore isn't loaded yet,
  // the request should fail outright and the JS Promise should be rejected
  // with an appropriate error.
  if (pending_request_ || !store) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&CredentialManager::RejectPromise,
                   weak_factory_.GetWeakPtr(), request_id,
                   pending_request_ ? ERROR_TYPE_PENDING_REQUEST
                                    : ERROR_TYPE_PASSWORD_STORE_UNAVAILABLE));
    return;
  }

  // If the page requested a zero-click credential -- one that can be returned
  // without first asking the user -- and if zero-click isn't currently
  // available, send back an empty credential.
  if (zero_click_only && !IsZeroClickAllowed()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&CredentialManager::SendCredentialByID,
                              weak_factory_.GetWeakPtr(), request_id,
                              password_manager::CredentialInfo()));
    return;
  }

  // If the page origin is untrusted, the request should be rejected.
  GURL page_url;
  if (!GetUrlWithAbsoluteTrust(&page_url)) {
    RejectPromise(request_id, ERROR_TYPE_SECURITY_ERROR_UNTRUSTED_ORIGIN);
    return;
  }

  // Bundle up the arguments and forward them to the PasswordStore, which will
  // asynchronously return the resulting Credential by invoking
  // |SendCredential|.
  std::vector<GURL> federation_urls;
  for (const auto& federation : federations)
    federation_urls.push_back(GURL(federation));
  std::vector<std::string> realms;
  pending_request_.reset(
      new password_manager::CredentialManagerPendingRequestTask(
          this, base::Bind(&CredentialManager::SendCredentialByID,
                           base::Unretained(this), request_id),
          zero_click_only, page_url, true, federation_urls, realms));
  store->GetAutofillableLogins(pending_request_.get());
}

void CredentialManager::SignedIn(int request_id,
                                 const GURL& source_url,
                                 const web::Credential& credential) {
  // Invoked when the page invokes navigator.credentials.notifySignedIn(), this
  // function stores the signed-in |credential| and sends a message back to the
  // page to resolve the Promise associated with |request_id|.
  DCHECK(credential.type != web::CredentialType::CREDENTIAL_TYPE_EMPTY);
  DCHECK_GE(request_id, 0);

  // Requests from untrusted origins should be rejected.
  GURL page_url;
  if (!GetUrlWithAbsoluteTrust(&page_url)) {
    RejectPromise(request_id, ERROR_TYPE_SECURITY_ERROR_UNTRUSTED_ORIGIN);
    return;
  }

  // Notify the page that the notification was successful to avoid blocking the
  // page while storing the credential. This is okay because the notification
  // doesn't imply that the credential will be stored, just that it might be.
  // It isn't the page's concern to know whether the storage took place or not.
  ResolvePromise(request_id);

  // Do nothing if the password manager isn't active.
  if (!client_->IsSavingAndFillingEnabledForCurrentPage())
    return;

  // Store the signed-in credential so that the user can save it, if desired.
  // Prompting the user and saving are handled by the PasswordFormManager.
  std::unique_ptr<autofill::PasswordForm> form(
      password_manager::CreatePasswordFormFromCredentialInfo(
          CredentialInfoFromWebCredential(credential), page_url));
  form->skip_zero_click = !IsZeroClickAllowed();

  // TODO(mkwst): This is a stub; we should be checking the PasswordStore to
  // determine whether or not the credential exists, and calling UpdateLogin
  // accordingly.
  form_manager_.reset(
      new password_manager::CredentialManagerPasswordFormManager(
          client_, driver_->AsWeakPtr(),
          *password_manager::CreateObservedPasswordFormFromOrigin(page_url),
          std::move(form), this));
}

void CredentialManager::SignedOut(int request_id, const GURL& source_url) {
  // Invoked when the page invokes navigator.credentials.notifySignedOut, this
  // function notifies the PasswordStore that zero-click sign-in should be
  // disabled for the current page origin.
  DCHECK_GE(request_id, 0);

  // Requests from untrusted origins should be rejected.
  GURL page_url;
  if (!GetUrlWithAbsoluteTrust(&page_url)) {
    RejectPromise(request_id, ERROR_TYPE_SECURITY_ERROR_UNTRUSTED_ORIGIN);
    return;
  }

  // The user signed out of the current page, so future zero-click credential
  // requests for this page should fail: otherwise, the next time the user
  // visits the page, if zero-click requests succeeded, the user might be auto-
  // signed-in again with the credential that they just signed out. Forward this
  // information to the PasswordStore via an asynchronous task.
  password_manager::PasswordStore* store = GetPasswordStore();
  if (store) {
    if (!pending_require_user_mediation_) {
      pending_require_user_mediation_.reset(
          new password_manager::
              CredentialManagerPendingRequireUserMediationTask(this));
    }
    password_manager::PasswordStore::FormDigest form = {
        autofill::PasswordForm::SCHEME_HTML, source_url.spec(), source_url};
    pending_require_user_mediation_->AddOrigin(form);
  }

  // Acknowledge the page's signOut notification without waiting for the
  // PasswordStore interaction to complete. The implementation of the sign-out
  // notification isn't the page's concern.
  ResolvePromise(request_id);
}

void CredentialManager::WebStateDestroyed() {
  // When the WebState is destroyed, clean up everything that depends on it.
  js_manager_.reset();
}

bool CredentialManager::IsZeroClickAllowed() const {
  // Zero-click sign-in is only allowed when the user hasn't turned it off and
  // when the user isn't in incognito mode.
  return *zero_click_sign_in_enabled_ && !client_->IsOffTheRecord();
}

GURL CredentialManager::GetOrigin() const {
  web::URLVerificationTrustLevel trust_level =
      web::URLVerificationTrustLevel::kNone;
  const GURL page_url(web_state()->GetCurrentURL(&trust_level));
  DCHECK_EQ(trust_level, web::URLVerificationTrustLevel::kAbsolute);
  return page_url;
}

void CredentialManager::SendCredential(
    const password_manager::SendCredentialCallback& send_callback,
    const password_manager::CredentialInfo& credential) {
  send_callback.Run(credential);
}

void CredentialManager::SendCredentialByID(
    int request_id,
    const password_manager::CredentialInfo& credential) {
  // Invoked when the asynchronous interaction with the PasswordStore completes,
  // this function forwards a |credential| back to the page via |js_manager_| by
  // resolving the JavaScript Promise associated with |request_id|.
  base::WeakPtr<CredentialManager> weak_this = weak_factory_.GetWeakPtr();
  [js_manager_
      resolvePromiseWithRequestID:request_id
                       credential:WebCredentialFromCredentialInfo(credential)
                completionHandler:^(BOOL) {
                  if (weak_this)
                    weak_this->pending_request_.reset();
                }];
}

void CredentialManager::SendPasswordForm(
    const password_manager::SendCredentialCallback& send_callback,
    const autofill::PasswordForm* form) {
  password_manager::CredentialInfo info;
  if (form) {
    password_manager::CredentialType type_to_return =
        form->federation_origin.unique()
            ? password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD
            : password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED;
    info = password_manager::CredentialInfo(*form, type_to_return);
    // TODO(vasilii): update |skip_zero_click| in the store (crbug.com/594110).
  }
  SendCredential(send_callback, info);
}

password_manager::PasswordManagerClient* CredentialManager::client() const {
  return client_;
}

password_manager::PasswordStore::FormDigest
CredentialManager::GetSynthesizedFormForOrigin() const {
  password_manager::PasswordStore::FormDigest form = {
      autofill::PasswordForm::SCHEME_HTML, std::string(),
      web_state()->GetLastCommittedURL().GetOrigin()};
  form.signon_realm = form.origin.spec();
  return form;
}

void CredentialManager::OnProvisionalSaveComplete() {
  // Invoked after a credential sent up by the page was stored in a FormManager
  // by |SignedIn|, this function asks the user if the password should be stored
  // in the password manager.
  DCHECK(form_manager_);
  if (client_->IsSavingAndFillingEnabledForCurrentPage() &&
      !form_manager_->IsBlacklisted()) {
    client_->PromptUserToSaveOrUpdatePassword(
        std::move(form_manager_),
        password_manager::CredentialSourceType::CREDENTIAL_SOURCE_API, false);
  }
}

password_manager::PasswordStore* CredentialManager::GetPasswordStore() {
  return client_ ? client_->GetPasswordStore() : nullptr;
}

void CredentialManager::DoneRequiringUserMediation() {
  // Invoked when the PasswordStore has finished processing the list of origins
  // that should have zero-click sign-in disabled.
  DCHECK(pending_require_user_mediation_);
  pending_require_user_mediation_.reset();
}

void CredentialManager::ResolvePromise(int request_id) {
  [js_manager_ resolvePromiseWithRequestID:request_id completionHandler:nil];
}

void CredentialManager::RejectPromise(int request_id, ErrorType error_type) {
  NSString* type = nil;
  NSString* message = nil;
  switch (error_type) {
    case ERROR_TYPE_PENDING_REQUEST:
      type = base::SysUTF8ToNSString(kCredentialsPendingRequestErrorType);
      message = base::SysUTF8ToNSString(kCredentialsPendingRequestErrorMessage);
      break;
    case ERROR_TYPE_PASSWORD_STORE_UNAVAILABLE:
      type = base::SysUTF8ToNSString(
          kCredentialsPasswordStoreUnavailableErrorType);
      message = base::SysUTF8ToNSString(
          kCredentialsPasswordStoreUnavailableErrorMessage);
      break;
    case ERROR_TYPE_SECURITY_ERROR_UNTRUSTED_ORIGIN:
      type = base::SysUTF8ToNSString(kCredentialsSecurityErrorType);
      message = base::SysUTF8ToNSString(
          kCredentialsSecurityErrorMessageUntrustedOrigin);
      break;
  }
  [js_manager_ rejectPromiseWithRequestID:request_id
                                errorType:type
                                  message:message
                        completionHandler:nil];
}

bool CredentialManager::GetUrlWithAbsoluteTrust(GURL* page_url) {
  web::URLVerificationTrustLevel trust_level =
      web::URLVerificationTrustLevel::kNone;
  const GURL possibly_untrusted_url(web_state()->GetCurrentURL(&trust_level));
  if (trust_level == web::URLVerificationTrustLevel::kAbsolute) {
    *page_url = possibly_untrusted_url;
    return true;
  }
  return false;
}
