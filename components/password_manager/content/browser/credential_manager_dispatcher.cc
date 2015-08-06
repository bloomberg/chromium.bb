// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/credential_manager_dispatcher.h"

#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"

namespace password_manager {

// CredentialManagerDispatcher -------------------------------------------------

CredentialManagerDispatcher::CredentialManagerDispatcher(
    content::WebContents* web_contents,
    PasswordManagerClient* client)
    : WebContentsObserver(web_contents), client_(client) {
  DCHECK(web_contents);
  auto_signin_enabled_.Init(prefs::kPasswordManagerAutoSignin,
                            client_->GetPrefs());
}

CredentialManagerDispatcher::~CredentialManagerDispatcher() {
}

bool CredentialManagerDispatcher::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CredentialManagerDispatcher, message)
    IPC_MESSAGE_HANDLER(CredentialManagerHostMsg_Store, OnStore);
    IPC_MESSAGE_HANDLER(CredentialManagerHostMsg_RequireUserMediation,
                        OnRequireUserMediation);
    IPC_MESSAGE_HANDLER(CredentialManagerHostMsg_RequestCredential,
                        OnRequestCredential);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CredentialManagerDispatcher::OnStore(
    int request_id,
    const password_manager::CredentialInfo& credential) {
  DCHECK(credential.type != CredentialType::CREDENTIAL_TYPE_EMPTY);
  DCHECK(request_id);
  web_contents()->GetRenderViewHost()->Send(
      new CredentialManagerMsg_AcknowledgeStore(
          web_contents()->GetRenderViewHost()->GetRoutingID(), request_id));

  if (!client_->IsSavingEnabledForCurrentPage())
    return;

  scoped_ptr<autofill::PasswordForm> form(CreatePasswordFormFromCredentialInfo(
      credential, web_contents()->GetLastCommittedURL().GetOrigin()));
  form->skip_zero_click = !IsZeroClickAllowed();

  // TODO(mkwst): This is a stub; we should be checking the PasswordStore to
  // determine whether or not the credential exists, and calling UpdateLogin
  // accordingly.
  form_manager_.reset(new CredentialManagerPasswordFormManager(
      client_, GetDriver(), *form, this));
}

void CredentialManagerDispatcher::OnProvisionalSaveComplete() {
  DCHECK(form_manager_);
  if (client_->IsSavingEnabledForCurrentPage() &&
      !form_manager_->IsBlacklisted()) {
    client_->PromptUserToSaveOrUpdatePassword(
        form_manager_.Pass(), CredentialSourceType::CREDENTIAL_SOURCE_API,
        false);
  }
}

void CredentialManagerDispatcher::OnRequireUserMediation(int request_id) {
  DCHECK(request_id);

  PasswordStore* store = GetPasswordStore();
  if (store) {
    if (!pending_require_user_mediation_) {
      pending_require_user_mediation_.reset(
          new CredentialManagerPendingRequireUserMediationTask(
              this, web_contents()->GetLastCommittedURL().GetOrigin()));

      // This will result in a callback to
      // CredentialManagerPendingRequireUserMediationTask::OnGetPasswordStoreResults().
      store->GetAutofillableLogins(pending_require_user_mediation_.get());
    } else {
      pending_require_user_mediation_->AddOrigin(
          web_contents()->GetLastCommittedURL().GetOrigin());
    }
  }

  web_contents()->GetRenderViewHost()->Send(
      new CredentialManagerMsg_AcknowledgeRequireUserMediation(
          web_contents()->GetRenderViewHost()->GetRoutingID(), request_id));
}

void CredentialManagerDispatcher::OnRequestCredential(
    int request_id,
    bool zero_click_only,
    const std::vector<GURL>& federations) {
  DCHECK(request_id);
  PasswordStore* store = GetPasswordStore();
  if (pending_request_ || !store) {
    web_contents()->GetRenderViewHost()->Send(
        new CredentialManagerMsg_RejectCredentialRequest(
            web_contents()->GetRenderViewHost()->GetRoutingID(), request_id,
            pending_request_
                ? blink::WebCredentialManagerError::ErrorTypePendingRequest
                : blink::WebCredentialManagerError::
                      ErrorTypePasswordStoreUnavailable));
    return;
  }

  if (zero_click_only && !IsZeroClickAllowed()) {
    web_contents()->GetRenderViewHost()->Send(
        new CredentialManagerMsg_SendCredential(
            web_contents()->GetRenderViewHost()->GetRoutingID(), request_id,
            CredentialInfo()));
    return;
  }

  pending_request_.reset(new CredentialManagerPendingRequestTask(
      this, request_id, zero_click_only,
      web_contents()->GetLastCommittedURL().GetOrigin(), federations));

  // This will result in a callback to
  // PendingRequestTask::OnGetPasswordStoreResults().
  store->GetAutofillableLogins(pending_request_.get());
}

PasswordStore* CredentialManagerDispatcher::GetPasswordStore() {
  return client_ ? client_->GetPasswordStore() : nullptr;
}

bool CredentialManagerDispatcher::IsZeroClickAllowed() const {
  return *auto_signin_enabled_ && !client_->IsOffTheRecord();
}

GURL CredentialManagerDispatcher::GetOrigin() const {
  return web_contents()->GetLastCommittedURL().GetOrigin();
}

base::WeakPtr<PasswordManagerDriver> CredentialManagerDispatcher::GetDriver() {
  ContentPasswordManagerDriverFactory* driver_factory =
      ContentPasswordManagerDriverFactory::FromWebContents(web_contents());
  DCHECK(driver_factory);
  PasswordManagerDriver* driver =
      driver_factory->GetDriverForFrame(web_contents()->GetMainFrame());
  return driver->AsWeakPtr();
}

void CredentialManagerDispatcher::SendCredential(int request_id,
                                                 const CredentialInfo& info) {
  DCHECK(pending_request_);
  DCHECK_EQ(pending_request_->id(), request_id);

  if (PasswordStore* store = GetPasswordStore()) {
    if (info.type != CredentialType::CREDENTIAL_TYPE_EMPTY &&
        IsZeroClickAllowed()) {
      scoped_ptr<autofill::PasswordForm> form(
          CreatePasswordFormFromCredentialInfo(info,
                                               pending_request_->origin()));
      form->skip_zero_click = false;
      store->UpdateLogin(*form);
    }
  }

  web_contents()->GetRenderViewHost()->Send(
      new CredentialManagerMsg_SendCredential(
          web_contents()->GetRenderViewHost()->GetRoutingID(),
          pending_request_->id(), info));
  pending_request_.reset();
}

PasswordManagerClient* CredentialManagerDispatcher::client() const {
  return client_;
}

void CredentialManagerDispatcher::DoneRequiringUserMediation() {
  DCHECK(pending_require_user_mediation_);
  pending_require_user_mediation_.reset();
}

}  // namespace password_manager
