// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_DISPATCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_DISPATCHER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/credential_manager_dispatcher.h"
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

class ContentCredentialManagerDispatcher : public CredentialManagerDispatcher,
                                           public content::WebContentsObserver,
                                           public PasswordStoreConsumer {
 public:
  ContentCredentialManagerDispatcher(content::WebContents* web_contents,
                                     PasswordManagerClient* client);
  ~ContentCredentialManagerDispatcher() override;

  void OnProvisionalSaveComplete();

  // CredentialManagerDispatcher implementation.
  void OnNotifyFailedSignIn(int request_id,
                            const password_manager::CredentialInfo&) override;
  void OnNotifySignedIn(int request_id,
                        const password_manager::CredentialInfo&) override;
  void OnNotifySignedOut(int request_id) override;
  void OnRequestCredential(int request_id,
                           bool zero_click_only,
                           const std::vector<GURL>& federations) override;

  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // PasswordStoreConsumer implementation.
  void OnGetPasswordStoreResults(
      const std::vector<autofill::PasswordForm*>& results) override;

  using CredentialCallback =
      base::Callback<void(const autofill::PasswordForm&)>;

 private:
  struct PendingRequestParameters;

  PasswordStore* GetPasswordStore();

  // Returns the driver for the current main frame.
  // Virtual for testing.
  virtual base::WeakPtr<PasswordManagerDriver> GetDriver();

  void SendCredential(int request_id, const CredentialInfo& info);

  PasswordManagerClient* client_;
  scoped_ptr<CredentialManagerPasswordFormManager> form_manager_;

  // When 'OnRequestCredential' is called, it in turn calls out to the
  // PasswordStore; we store request details here in order to properly
  // respond to the request once the PasswordStore gives us data.
  scoped_ptr<PendingRequestParameters> pending_request_;

  DISALLOW_COPY_AND_ASSIGN(ContentCredentialManagerDispatcher);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_DISPATCHER_H_
