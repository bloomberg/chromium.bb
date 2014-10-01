// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_DISPATCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_DISPATCHER_H_

#include "base/macros.h"
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

class PasswordManagerClient;
class PasswordStore;
struct CredentialInfo;

class ContentCredentialManagerDispatcher : public CredentialManagerDispatcher,
                                           public content::WebContentsObserver,
                                           public PasswordStoreConsumer {
 public:
  // |client| isn't yet used by this class, but is necessary for the next step:
  // wiring this up as a subclass of PasswordStoreConsumer.
  ContentCredentialManagerDispatcher(content::WebContents* web_contents,
                                     PasswordManagerClient* client);
  virtual ~ContentCredentialManagerDispatcher();

  // CredentialManagerDispatcher implementation.
  virtual void OnNotifyFailedSignIn(
      int request_id,
      const password_manager::CredentialInfo&) override;
  virtual void OnNotifySignedIn(
      int request_id,
      const password_manager::CredentialInfo&) override;
  virtual void OnNotifySignedOut(int request_id) override;
  virtual void OnRequestCredential(
      int request_id,
      bool zero_click_only,
      const std::vector<GURL>& federations) override;

  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) override;

  // PasswordStoreConsumer implementation.
  virtual void OnGetPasswordStoreResults(
      const std::vector<autofill::PasswordForm*>& results) override;

 private:
  PasswordStore* GetPasswordStore();

  PasswordManagerClient* client_;

  // When 'OnRequestCredential' is called, it in turn calls out to the
  // PasswordStore; we store the request ID here in order to properly respond
  // to the request once the PasswordStore gives us data.
  int pending_request_id_;

  DISALLOW_COPY_AND_ASSIGN(ContentCredentialManagerDispatcher);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_DISPATCHER_H_
