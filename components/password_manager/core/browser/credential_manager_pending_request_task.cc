// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "url/gurl.h"

namespace password_manager {

CredentialManagerPendingRequestTask::CredentialManagerPendingRequestTask(
    CredentialManagerPendingRequestTaskDelegate* delegate,
    int request_id,
    bool request_zero_click_only,
    const GURL& request_origin,
    const std::vector<GURL>& request_federations)
    : delegate_(delegate),
      id_(request_id),
      zero_click_only_(request_zero_click_only),
      origin_(request_origin) {
  for (const GURL& origin : request_federations)
    federations_.insert(origin.spec());
}

CredentialManagerPendingRequestTask::~CredentialManagerPendingRequestTask() =
    default;

void CredentialManagerPendingRequestTask::OnGetPasswordStoreResults(
    ScopedVector<autofill::PasswordForm> results) {
  if (delegate_->GetOrigin() != origin_) {
    delegate_->SendCredential(id_, CredentialInfo());
    return;
  }

  ScopedVector<autofill::PasswordForm> local_results;
  ScopedVector<autofill::PasswordForm> federated_results;
  const autofill::PasswordForm* zero_click_form_to_return = nullptr;
  bool found_zero_clickable_credential = false;
  for (auto& form : results) {
    // PasswordFrom and GURL have different definition of origin.
    // PasswordForm definition: scheme, host, port and path.
    // GURL definition: scheme, host, and port.
    // So we can't compare them directly.
    if (form->origin.GetOrigin() == origin_.GetOrigin()) {
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

  if ((local_results.empty() && federated_results.empty())) {
    delegate_->SendCredential(id_, CredentialInfo());
    return;
  }

  if (zero_click_form_to_return && delegate_->IsZeroClickAllowed()) {
    auto it = std::find(local_results.begin(), local_results.end(),
                        zero_click_form_to_return);
    CredentialInfo info(*zero_click_form_to_return,
                        zero_click_form_to_return->federation_url.is_empty()
                            ? CredentialType::CREDENTIAL_TYPE_PASSWORD
                            : CredentialType::CREDENTIAL_TYPE_FEDERATED);
    DCHECK(it != local_results.end());
    std::swap(*it, local_results[0]);
    // Clear the form pointer since its owner is being passed.
    zero_click_form_to_return = nullptr;
    delegate_->client()->NotifyUserAutoSignin(local_results.Pass());
    delegate_->SendCredential(id_, info);
    return;
  }

  if (zero_click_only_ ||
      !delegate_->client()->PromptUserToChooseCredentials(
          local_results.Pass(), federated_results.Pass(), origin_,
          base::Bind(
              &CredentialManagerPendingRequestTaskDelegate::SendCredential,
              base::Unretained(delegate_), id_))) {
    delegate_->SendCredential(id_, CredentialInfo());
  }
}

}  // namespace password_manager
