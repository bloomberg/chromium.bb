// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_DISPATCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_DISPATCHER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
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

class CredentialManagerDispatcher : public content::WebContentsObserver {
 public:
  CredentialManagerDispatcher(content::WebContents* web_contents,
                              PasswordManagerClient* client);
  ~CredentialManagerDispatcher() override;

  void OnProvisionalSaveComplete();

  // Called in response to an IPC from the renderer, triggered by a page's call
  // to 'navigator.credentials.notifyFailedSignIn'.
  virtual void OnNotifyFailedSignIn(int request_id,
                                    const password_manager::CredentialInfo&);

  // Called in response to an IPC from the renderer, triggered by a page's call
  // to 'navigator.credentials.notifySignedIn'.
  virtual void OnNotifySignedIn(int request_id,
                                const password_manager::CredentialInfo&);

  // Called in response to an IPC from the renderer, triggered by a page's call
  // to 'navigator.credentials.notifySignedOut'.
  virtual void OnNotifySignedOut(int request_id);

  // Called in response to an IPC from the renderer, triggered by a page's call
  // to 'navigator.credentials.request'.
  //
  // TODO(vabr): Determine if we can drop the `const` here to save some copies
  // while processing the request.
  virtual void OnRequestCredential(int request_id,
                                   bool zero_click_only,
                                   const std::vector<GURL>& federations);

  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  using CredentialCallback =
      base::Callback<void(const autofill::PasswordForm&)>;

  PasswordManagerClient* client() const { return client_; }

 private:
  class PendingRequestTask;
  class PendingSignedOutTask;

  PasswordStore* GetPasswordStore();

  bool IsSavingEnabledForCurrentPage() const;
  bool IsZeroClickAllowed() const;

  // Returns the driver for the current main frame.
  // Virtual for testing.
  virtual base::WeakPtr<PasswordManagerDriver> GetDriver();

  void SendCredential(int request_id, const CredentialInfo& info);
  void DoneSigningOut();

  PasswordManagerClient* client_;
  scoped_ptr<CredentialManagerPasswordFormManager> form_manager_;

  // Set to false to disable automatic signing in.
  BooleanPrefMember auto_signin_enabled_;

  // When 'OnRequestCredential' is called, it in turn calls out to the
  // PasswordStore; we push enough data into Pending*Task objects so that
  // they can properly respond to the request once the PasswordStore gives
  // us data.
  scoped_ptr<PendingRequestTask> pending_request_;
  scoped_ptr<PendingSignedOutTask> pending_sign_out_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerDispatcher);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_DISPATCHER_H_
