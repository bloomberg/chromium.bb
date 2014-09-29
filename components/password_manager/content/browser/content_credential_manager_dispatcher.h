// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_DISPATCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_DISPATCHER_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/credential_manager_dispatcher.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {
class WebContents;
}

namespace password_manager {

class PasswordManagerClient;
struct CredentialInfo;

class ContentCredentialManagerDispatcher : public CredentialManagerDispatcher,
                                           public content::WebContentsObserver {
 public:
  // |client| isn't yet used by this class, but is necessary for the next step:
  // wiring this up as a subclass of PasswordStoreConsumer.
  ContentCredentialManagerDispatcher(content::WebContents* web_contents,
                                     PasswordManagerClient* client);
  virtual ~ContentCredentialManagerDispatcher();

  // CredentialManagerDispatcher implementation.
  virtual void OnNotifyFailedSignIn(
      int request_id,
      const password_manager::CredentialInfo&) OVERRIDE;
  virtual void OnNotifySignedIn(
      int request_id,
      const password_manager::CredentialInfo&) OVERRIDE;
  virtual void OnNotifySignedOut(int request_id) OVERRIDE;
  virtual void OnRequestCredential(
      int request_id,
      bool zero_click_only,
      const std::vector<GURL>& federations) OVERRIDE;

  // content::WebContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  PasswordManagerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(ContentCredentialManagerDispatcher);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_DISPATCHER_H_
