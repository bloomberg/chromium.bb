// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_CREDENTIAL_MANAGER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_CREDENTIAL_MANAGER_H_

#include <string>
#include <vector>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/credential_manager_password_form_manager.h"
#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"
#include "components/password_manager/core/browser/credential_manager_pending_require_user_mediation_task.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_member.h"
#include "ios/web/public/web_state/web_state_observer.h"

@class JSCredentialManager;

// Implements the app-side of the CredentialManagement JavaScript API.
// Injects and listens to the injected JavaScript, owns and drives the user
// interface, and integrates with the password manager. This is the iOS
// equivalent of the upstream class CredentialManagerDispatcher. Note: Only
// activates on iOS 8 and later.
class CredentialManager
    : public password_manager::CredentialManagerPasswordFormManagerDelegate,
      public password_manager::CredentialManagerPendingRequestTaskDelegate,
      public password_manager::
          CredentialManagerPendingRequireUserMediationTaskDelegate,
      public web::WebStateObserver {
 public:
  CredentialManager(web::WebState* web_state,
                    password_manager::PasswordManagerClient* client,
                    password_manager::PasswordManagerDriver* driver,
                    JSCredentialManager* js_manager);
  ~CredentialManager() override;

  // web::WebStateObserver:
  void PageLoaded(
      web::PageLoadCompletionStatus load_completion_status) override;
  void CredentialsRequested(int request_id,
                            const GURL& source_url,
                            bool zero_click_only,
                            const std::vector<std::string>& federations,
                            bool is_user_initiated) override;
  void SignedIn(int request_id,
                const GURL& source_url,
                const web::Credential& credential) override;
  void SignedOut(int request_id, const GURL& source_url) override;
  void WebStateDestroyed() override;

  // password_manager::CredentialManagerPendingRequestTaskDelegate:
  bool IsZeroClickAllowed() const override;
  GURL GetOrigin() const override;
  void SendCredential(
      int id,
      const password_manager::CredentialInfo& credential) override;
  password_manager::PasswordManagerClient* client() const override;
  autofill::PasswordForm GetSynthesizedFormForOrigin() const override;

  // password_manager::CredentialManagerPendingRequireUserMediationTaskDelegate:
  password_manager::PasswordStore* GetPasswordStore() override;
  void DoneRequiringUserMediation() override;

  // CredentialManagerPasswordFormManagerDelegate:
  void OnProvisionalSaveComplete() override;

 private:
  // The errors that can cause a request to fail.
  enum ErrorType {
    // An existing request is outstanding.
    ERROR_TYPE_PENDING_REQUEST = 0,

    // The password store isn't available.
    ERROR_TYPE_PASSWORD_STORE_UNAVAILABLE,

    // The page origin is untrusted.
    ERROR_TYPE_SECURITY_ERROR_UNTRUSTED_ORIGIN,
  };

  // Sends a message via |js_manager_| to resolve the JavaScript Promise
  // associated with |request_id|. Invoked after a page-initiated credential
  // event is acknowledged by the PasswordStore.
  void ResolvePromise(int request_id);

  // Sends a message via |js_manager_| to reject the JavaScript Promise
  // associated with |request_id_| with the given |error_type|. Invoked after a
  // page-initiated credential event, store, or retrieval fails.
  void RejectPromise(int request_id, ErrorType error_type);

  // Determines the currently loaded page's URL from the active WebState, but
  // only if it is absolutely trusted. Does not hit the network, but still might
  // be costly depending on the webview. Returns true if successful.
  bool GetUrlWithAbsoluteTrust(GURL* page_url);

  // The request to retrieve credentials from the PasswordStore.
  scoped_ptr<password_manager::CredentialManagerPendingRequestTask>
      pending_request_;

  // The task to notify the password manager that the user was signed out.
  scoped_ptr<password_manager::CredentialManagerPendingRequireUserMediationTask>
      pending_require_user_mediation_;

  // Saves credentials to the PasswordStore.
  scoped_ptr<password_manager::CredentialManagerPasswordFormManager>
      form_manager_;

  // Injected JavaScript to provide the API to web pages.
  base::scoped_nsobject<JSCredentialManager> js_manager_;

  // Client to access Chrome-specific password manager functionality. Weak.
  password_manager::PasswordManagerClient* client_;

  // Driver to access embedder-specific password manager functionality. Weak.
  password_manager::PasswordManagerDriver* driver_;

  // Whether zero-click sign-in is enabled.
  BooleanPrefMember zero_click_sign_in_enabled_;

  // Weak pointer factory for asynchronously resolving requests.
  base::WeakPtrFactory<CredentialManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManager);
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_CREDENTIAL_MANAGER_H_
