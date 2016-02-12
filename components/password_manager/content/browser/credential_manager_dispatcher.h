// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_DISPATCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_DISPATCHER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/credential_manager_password_form_manager.h"
#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"
#include "components/password_manager/core/browser/credential_manager_pending_require_user_mediation_task.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace autofill {
struct PasswordForm;
}

namespace content {
class WebContents;
}

namespace password_manager {
class CredentialManagerPasswordFormManager;
class PasswordManagerClient;
class PasswordManagerDriver;
class PasswordStore;
struct CredentialInfo;

class CredentialManagerDispatcher
    : public content::WebContentsObserver,
      public CredentialManagerPasswordFormManagerDelegate,
      public CredentialManagerPendingRequestTaskDelegate,
      public CredentialManagerPendingRequireUserMediationTaskDelegate {
 public:
  CredentialManagerDispatcher(content::WebContents* web_contents,
                              PasswordManagerClient* client);
  ~CredentialManagerDispatcher() override;

  // Called in response to an IPC from the renderer, triggered by a page's call
  // to 'navigator.credentials.store'.
  virtual void OnStore(int request_id, const password_manager::CredentialInfo&);

  // Called in response to an IPC from the renderer, triggered by a page's call
  // to 'navigator.credentials.requireUserMediation'.
  virtual void OnRequireUserMediation(int request_id);

  // Called in response to an IPC from the renderer, triggered by a page's call
  // to 'navigator.credentials.request'.
  //
  // TODO(vabr): Determine if we can drop the `const` here to save some copies
  // while processing the request.
  virtual void OnRequestCredential(int request_id,
                                   bool zero_click_only,
                                   bool include_passwords,
                                   const std::vector<GURL>& federations);

  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  using CredentialCallback =
      base::Callback<void(const autofill::PasswordForm&)>;

  // CredentialManagerPendingRequestTaskDelegate:
  bool IsZeroClickAllowed() const override;
  GURL GetOrigin() const override;
  void SendCredential(int request_id, const CredentialInfo& info) override;
  PasswordManagerClient* client() const override;
  autofill::PasswordForm GetSynthesizedFormForOrigin() const override;

  // CredentialManagerPendingSignedOutTaskDelegate:
  PasswordStore* GetPasswordStore() override;
  void DoneRequiringUserMediation() override;

  // CredentialManagerPasswordFormManagerDelegate:
  void OnProvisionalSaveComplete() override;

 private:
  // Returns the driver for the current main frame.
  // Virtual for testing.
  virtual base::WeakPtr<PasswordManagerDriver> GetDriver();

  // Schedules a CredentiaManagerPendingRequestTask (during
  // |OnRequestCredential()|) after the PasswordStore's AffiliationMatchHelper
  // grabs a list of realms related to the current web origin.
  void ScheduleRequestTask(int request_id,
                           bool zero_click_only,
                           bool include_passwords,
                           const std::vector<GURL>& federations,
                           const std::vector<std::string>& android_realms);

  // Schedules a CredentialManagerPendingRequireUserMediationTask after the
  // AffiliationMatchHelper grabs a list of realms related to the current
  // web origin.
  void ScheduleRequireMediationTask(
      int request_id,
      const std::vector<std::string>& android_realms);

  PasswordManagerClient* client_;
  scoped_ptr<CredentialManagerPasswordFormManager> form_manager_;

  // Set to false to disable automatic signing in.
  BooleanPrefMember auto_signin_enabled_;

  // When 'OnRequestCredential' is called, it in turn calls out to the
  // PasswordStore; we push enough data into Pending*Task objects so that
  // they can properly respond to the request once the PasswordStore gives
  // us data.
  scoped_ptr<CredentialManagerPendingRequestTask> pending_request_;
  scoped_ptr<CredentialManagerPendingRequireUserMediationTask>
      pending_require_user_mediation_;

  base::WeakPtrFactory<CredentialManagerDispatcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerDispatcher);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_DISPATCHER_H_
