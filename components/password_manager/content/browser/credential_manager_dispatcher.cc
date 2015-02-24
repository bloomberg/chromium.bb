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
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"

namespace password_manager {

// CredentialManagerDispatcher::PendingRequestTask -----------------------------

class CredentialManagerDispatcher::PendingRequestTask
    : public PasswordStoreConsumer {
 public:
  // TODO(mkwst): De-inline the methods in this class. http://goo.gl/RmFwKd
  PendingRequestTask(CredentialManagerDispatcher* const dispatcher,
                     int request_id,
                     bool request_zero_click_only,
                     const GURL& request_origin,
                     const std::vector<GURL>& request_federations)
      : dispatcher_(dispatcher),
        id_(request_id),
        zero_click_only_(request_zero_click_only),
        origin_(request_origin) {
    for (const GURL& origin : request_federations)
      federations_.insert(origin.spec());
  }

  int id() const { return id_; }
  const GURL& origin() const { return origin_; }

  // PasswordStoreConsumer implementation.
  void OnGetPasswordStoreResults(
      ScopedVector<autofill::PasswordForm> results) override;

 private:
  // Backlink to the CredentialManagerDispatcher that owns this object.
  CredentialManagerDispatcher* const dispatcher_;

  const int id_;
  const bool zero_click_only_;
  const GURL origin_;
  std::set<std::string> federations_;

  DISALLOW_COPY_AND_ASSIGN(PendingRequestTask);
};

void CredentialManagerDispatcher::PendingRequestTask::OnGetPasswordStoreResults(
    ScopedVector<autofill::PasswordForm> results) {
  ScopedVector<autofill::PasswordForm> local_results;
  ScopedVector<autofill::PasswordForm> federated_results;
  autofill::PasswordForm* zero_click_form_to_return = nullptr;
  bool found_zero_clickable_credential = false;
  for (auto& form : results) {
    if (form->origin == origin_) {
      local_results.push_back(form);

      // If this is a zero-clickable PasswordForm, and we haven't found any
      // other zero-clickable PasswordForms, then store this one for later.
      // If we have found other zero-clickable PasswordForms, then clear
      // the stored form (we return zero-click forms iff there is a single,
      // unambigious choice).
      if (!form->skip_zero_click) {
        zero_click_form_to_return =
            found_zero_clickable_credential ? nullptr : form;
        found_zero_clickable_credential = true;
      }
      form = nullptr;
    }

    // TODO(mkwst): We're debating whether or not federations ought to be
    // available at this point, as it's not clear that the user experience
    // is at all reasonable. Until that's resolved, we'll drop the forms that
    // match |federations_| on the floor rather than pushing them into
    // 'federated_results'. Since we don't touch the reference in |results|,
    // they will be safely deleted after this task executes.
  }

  if ((local_results.empty() && federated_results.empty()) ||
      dispatcher_->web_contents()->GetLastCommittedURL().GetOrigin() !=
          origin_) {
    dispatcher_->SendCredential(id_, CredentialInfo());
    return;
  }
  if (zero_click_form_to_return && dispatcher_->IsZeroClickAllowed()) {
    CredentialInfo info(*zero_click_form_to_return,
                        zero_click_form_to_return->federation_url.is_empty()
                            ? CredentialType::CREDENTIAL_TYPE_LOCAL
                            : CredentialType::CREDENTIAL_TYPE_FEDERATED);
    auto it = std::find(local_results.begin(), local_results.end(),
                        zero_click_form_to_return);
    DCHECK(it != local_results.end());
    std::swap(*it, local_results[0]);
    dispatcher_->client()->NotifyUserAutoSignin(local_results.Pass());
    dispatcher_->SendCredential(id_, info);
    return;
  }

  if (zero_click_only_ ||
      !dispatcher_->client()->PromptUserToChooseCredentials(
          local_results.Pass(), federated_results.Pass(), origin_,
          base::Bind(&CredentialManagerDispatcher::SendCredential,
                     base::Unretained(dispatcher_), id_))) {
    dispatcher_->SendCredential(id_, CredentialInfo());
  }
}

// CredentialManagerDispatcher::PendingSignedOutTask ---------------------------

class CredentialManagerDispatcher::PendingSignedOutTask
    : public PasswordStoreConsumer {
 public:
  PendingSignedOutTask(CredentialManagerDispatcher* const dispatcher,
                       const GURL& origin);

  void AddOrigin(const GURL& origin);

  // PasswordStoreConsumer implementation.
  void OnGetPasswordStoreResults(
      ScopedVector<autofill::PasswordForm> results) override;

 private:
  // Backlink to the CredentialManagerDispatcher that owns this object.
  CredentialManagerDispatcher* const dispatcher_;
  std::set<std::string> origins_;

  DISALLOW_COPY_AND_ASSIGN(PendingSignedOutTask);
};

CredentialManagerDispatcher::PendingSignedOutTask::PendingSignedOutTask(
    CredentialManagerDispatcher* const dispatcher,
    const GURL& origin)
    : dispatcher_(dispatcher) {
  origins_.insert(origin.spec());
}

void CredentialManagerDispatcher::PendingSignedOutTask::AddOrigin(
    const GURL& origin) {
  origins_.insert(origin.spec());
}

void CredentialManagerDispatcher::PendingSignedOutTask::
    OnGetPasswordStoreResults(ScopedVector<autofill::PasswordForm> results) {
  PasswordStore* store = dispatcher_->GetPasswordStore();
  for (autofill::PasswordForm* form : results) {
    if (origins_.count(form->origin.spec())) {
      form->skip_zero_click = true;
      // Note that UpdateLogin ends up copying the form while posting a task to
      // update the PasswordStore, so it's fine to let |results| delete the
      // original at the end of this method.
      store->UpdateLogin(*form);
    }
  }

  dispatcher_->DoneSigningOut();
}

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
  DCHECK(credential.type != CredentialType::CREDENTIAL_TYPE_EMPTY);
  DCHECK(request_id);
  web_contents()->GetRenderViewHost()->Send(
      new CredentialManagerMsg_AcknowledgeSignedIn(
          web_contents()->GetRenderViewHost()->GetRoutingID(), request_id));

  if (!IsSavingEnabledForCurrentPage())
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
  client_->PromptUserToSavePassword(
      form_manager_.Pass(), CredentialSourceType::CREDENTIAL_SOURCE_API);
}

void CredentialManagerDispatcher::OnNotifySignedOut(int request_id) {
  DCHECK(request_id);

  PasswordStore* store = GetPasswordStore();
  if (store) {
    if (!pending_sign_out_) {
      pending_sign_out_.reset(new PendingSignedOutTask(
          this, web_contents()->GetLastCommittedURL().GetOrigin()));

      // This will result in a callback to
      // PendingSignedOutTask::OnGetPasswordStoreResults().
      store->GetAutofillableLogins(pending_sign_out_.get());
    } else {
      pending_sign_out_->AddOrigin(
          web_contents()->GetLastCommittedURL().GetOrigin());
    }
  }

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

  pending_request_.reset(new PendingRequestTask(
      this, request_id, zero_click_only,
      web_contents()->GetLastCommittedURL().GetOrigin(), federations));

  // This will result in a callback to
  // PendingRequestTask::OnGetPasswordStoreResults().
  store->GetAutofillableLogins(pending_request_.get());
}

PasswordStore* CredentialManagerDispatcher::GetPasswordStore() {
  return client_ ? client_->GetPasswordStore() : nullptr;
}

bool CredentialManagerDispatcher::IsSavingEnabledForCurrentPage() const {
  // TODO(vasilii): add more, see http://crbug.com/450583.
  return !client_->IsOffTheRecord();
}

bool CredentialManagerDispatcher::IsZeroClickAllowed() const {
  return *auto_signin_enabled_ && !client_->IsOffTheRecord();
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

void CredentialManagerDispatcher::DoneSigningOut() {
  DCHECK(pending_sign_out_);
  pending_sign_out_.reset();
}

}  // namespace password_manager
