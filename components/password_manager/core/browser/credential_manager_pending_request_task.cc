// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "url/gurl.h"

namespace password_manager {

CredentialManagerPendingRequestTask::CredentialManagerPendingRequestTask(
    CredentialManagerPendingRequestTaskDelegate* delegate,
    int request_id,
    bool request_zero_click_only,
    const GURL& request_origin,
    const std::vector<GURL>& request_federations,
    const std::vector<std::string>& affiliated_realms)
    : delegate_(delegate),
      id_(request_id),
      zero_click_only_(request_zero_click_only),
      origin_(request_origin),
      affiliated_realms_(affiliated_realms.begin(), affiliated_realms.end()) {
  CHECK(!delegate_->client()->DidLastPageLoadEncounterSSLErrors());
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
  ScopedVector<autofill::PasswordForm> affiliated_results;
  ScopedVector<autofill::PasswordForm> federated_results;
  const autofill::PasswordForm* zero_click_form_to_return = nullptr;
  for (auto& form : results) {
    // PasswordFrom and GURL have different definition of origin.
    // PasswordForm definition: scheme, host, port and path.
    // GURL definition: scheme, host, and port.
    // So we can't compare them directly.
    if (form->origin.GetOrigin() == origin_.GetOrigin()) {
      local_results.push_back(form);
      form = nullptr;
    } else if (affiliated_realms_.count(form->signon_realm) &&
               AffiliatedMatchHelper::IsValidAndroidCredential(*form)) {
      form->is_affiliation_based_match = true;
      affiliated_results.push_back(form);
      form = nullptr;
    }

    // TODO(mkwst): We're debating whether or not federations ought to be
    // available at this point, as it's not clear that the user experience
    // is at all reasonable. Until that's resolved, we'll drop the forms that
    // match |federations_| on the floor rather than pushing them into
    // 'federated_results'. Since we don't touch the reference in |results|,
    // they will be safely deleted after this task executes.
  }

  if (!affiliated_results.empty()) {
    // TODO(mkwst): This doesn't create a PasswordForm that we can use to create
    // a FederatedCredential (via CreatePasswordFormFromCredentialInfo). We need
    // to fix that.
    password_manager_util::TrimUsernameOnlyCredentials(&affiliated_results);
    local_results.insert(local_results.end(), affiliated_results.begin(),
                         affiliated_results.end());
    affiliated_results.weak_clear();
  }

  if ((local_results.empty() && federated_results.empty())) {
    delegate_->SendCredential(id_, CredentialInfo());
    return;
  }

  // We only perform zero-click sign-in when the result is completely
  // unambigious. If the user could theoretically choose from more than one
  // option, cancel zero-click.
  if (local_results.size() == 1u && !local_results[0]->skip_zero_click)
    zero_click_form_to_return = local_results[0];

  if (zero_click_form_to_return && delegate_->IsZeroClickAllowed()) {
    // TODO(mkwst): This is all too complex now. We should just be able to check
    // the first item in the list, since we're now only doing any of this work
    // when there's only one item. Kill it all with fire in a future CL.
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
