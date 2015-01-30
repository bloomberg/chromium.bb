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
#include "components/password_manager/content/browser/credential_manager_password_form_manager.h"
#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/password_manager/content/common/credential_manager_types.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_store.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"

namespace password_manager {

struct CredentialManagerDispatcher::PendingRequestParameters {
  PendingRequestParameters(int request_id,
                           bool request_zero_click_only,
                           GURL request_origin,
                           const std::vector<GURL>& request_federations)
      : id(request_id),
        zero_click_only(request_zero_click_only),
        origin(request_origin),
        federations(request_federations) {}

  int id;
  bool zero_click_only;
  GURL origin;
  std::vector<GURL> federations;
};

CredentialManagerDispatcher::CredentialManagerDispatcher(
    content::WebContents* web_contents,
    PasswordManagerClient* client)
    : WebContentsObserver(web_contents), client_(client) {
  DCHECK(web_contents);
}

CredentialManagerDispatcher::~CredentialManagerDispatcher() {
}

bool CredentialManagerDispatcher::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CredentialManagerDispatcher, message)
    IPC_MESSAGE_HANDLER(CredentialManagerHostMsg_NotifyFailedSignIn,
                        OnNotifyFailedSignIn);
    IPC_MESSAGE_HANDLER(CredentialManagerHostMsg_NotifySignedIn,
                        OnNotifySignedIn);
    IPC_MESSAGE_HANDLER(CredentialManagerHostMsg_NotifySignedOut,
                        OnNotifySignedOut);
    IPC_MESSAGE_HANDLER(CredentialManagerHostMsg_RequestCredential,
                        OnRequestCredential);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CredentialManagerDispatcher::OnNotifyFailedSignIn(int request_id,
                                                       const CredentialInfo&) {
  DCHECK(request_id);
  // TODO(mkwst): This is a stub.
  web_contents()->GetRenderViewHost()->Send(
      new CredentialManagerMsg_AcknowledgeFailedSignIn(
          web_contents()->GetRenderViewHost()->GetRoutingID(), request_id));
}

void CredentialManagerDispatcher::OnNotifySignedIn(
    int request_id,
    const password_manager::CredentialInfo& credential) {
  DCHECK(request_id);
  web_contents()->GetRenderViewHost()->Send(
      new CredentialManagerMsg_AcknowledgeSignedIn(
          web_contents()->GetRenderViewHost()->GetRoutingID(), request_id));

  if (!IsSavingEnabledForCurrentPage())
    return;

  scoped_ptr<autofill::PasswordForm> form(CreatePasswordFormFromCredentialInfo(
      credential, web_contents()->GetLastCommittedURL().GetOrigin()));

  // TODO(mkwst): This is a stub; we should be checking the PasswordStore to
  // determine whether or not the credential exists, and calling UpdateLogin
  // accordingly.
  form_manager_.reset(new CredentialManagerPasswordFormManager(
      client_, GetDriver(), *form, this));
}

void CredentialManagerDispatcher::OnProvisionalSaveComplete() {
  DCHECK(form_manager_);
  client_->PromptUserToSavePassword(form_manager_.Pass());
}

void CredentialManagerDispatcher::OnNotifySignedOut(int request_id) {
  DCHECK(request_id);
  // TODO(mkwst): This is a stub.
  web_contents()->GetRenderViewHost()->Send(
      new CredentialManagerMsg_AcknowledgeSignedOut(
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

  pending_request_.reset(new PendingRequestParameters(
      request_id, zero_click_only,
      web_contents()->GetLastCommittedURL().GetOrigin(), federations));

  // This will result in a callback to ::OnGetPasswordStoreResults().
  store->GetAutofillableLogins(this);
}

void CredentialManagerDispatcher::OnGetPasswordStoreResults(
    const std::vector<autofill::PasswordForm*>& results) {
  DCHECK(pending_request_);

  std::set<std::string> federations;
  for (const GURL& origin : pending_request_->federations)
    federations.insert(origin.spec());

  // We own the PasswordForm instances, so we're responsible for cleaning
  // up the instances we don't add to |local_results| or |federated_results|.
  std::vector<autofill::PasswordForm*> local_results;
  std::vector<autofill::PasswordForm*> federated_results;
  for (autofill::PasswordForm* form : results) {
    if (form->origin == pending_request_->origin)
      local_results.push_back(form);
    else if (federations.count(form->origin.spec()) != 0)
      federated_results.push_back(form);
    else
      delete form;
  }

  if ((local_results.empty() && federated_results.empty()) ||
      web_contents()->GetLastCommittedURL().GetOrigin() !=
          pending_request_->origin) {
    SendCredential(pending_request_->id, CredentialInfo());
    return;
  }

  if (local_results.size() == 1 && IsZeroClickAllowed()) {
    // TODO(mkwst): Use the `one_time_disable_zero_click` flag on the result
    // to prevent auto-sign-in, once that flag is implemented.
    CredentialInfo info(*local_results[0],
                        local_results[0]->federation_url.is_empty()
                            ? CredentialType::CREDENTIAL_TYPE_LOCAL
                            : CredentialType::CREDENTIAL_TYPE_FEDERATED);
    STLDeleteElements(&local_results);
    STLDeleteElements(&federated_results);
    SendCredential(pending_request_->id, info);
    return;
  }

  if (pending_request_->zero_click_only ||
      !client_->PromptUserToChooseCredentials(
          local_results, federated_results,
          base::Bind(&CredentialManagerDispatcher::SendCredential,
                     base::Unretained(this), pending_request_->id))) {
    STLDeleteElements(&local_results);
    STLDeleteElements(&federated_results);
    SendCredential(pending_request_->id, CredentialInfo());
  }
}

PasswordStore* CredentialManagerDispatcher::GetPasswordStore() {
  return client_ ? client_->GetPasswordStore() : nullptr;
}

bool CredentialManagerDispatcher::IsSavingEnabledForCurrentPage() const {
  // TODO(vasilii): add more, see http://crbug.com/450583.
  return !client_->IsOffTheRecord();
}

bool CredentialManagerDispatcher::IsZeroClickAllowed() const {
  return !client_->IsOffTheRecord() && client_->IsZeroClickEnabled();
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
  DCHECK_EQ(pending_request_->id, request_id);
  web_contents()->GetRenderViewHost()->Send(
      new CredentialManagerMsg_SendCredential(
          web_contents()->GetRenderViewHost()->GetRoutingID(),
          pending_request_->id, info));
  pending_request_.reset();
}

}  // namespace password_manager
