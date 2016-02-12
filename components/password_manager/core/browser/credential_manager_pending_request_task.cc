// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"

#include <utility>

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
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
    bool include_passwords,
    const std::vector<GURL>& request_federations,
    const std::vector<std::string>& affiliated_realms)
    : delegate_(delegate),
      id_(request_id),
      zero_click_only_(request_zero_click_only),
      origin_(request_origin),
      include_passwords_(include_passwords),
      affiliated_realms_(affiliated_realms.begin(), affiliated_realms.end()) {
  CHECK(!delegate_->client()->DidLastPageLoadEncounterSSLErrors());
  for (const GURL& federation : request_federations)
    federations_.insert(federation.GetOrigin().spec());
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
  for (auto& form : results) {
    // PasswordFrom and GURL have different definition of origin.
    // PasswordForm definition: scheme, host, port and path.
    // GURL definition: scheme, host, and port.
    // So we can't compare them directly.
    if (form->origin.GetOrigin() == origin_.GetOrigin()) {
      if ((form->federation_url.is_empty() && include_passwords_) ||
          (!form->federation_url.is_empty() &&
           federations_.count(form->federation_url.spec()))) {
        local_results.push_back(form);
        form = nullptr;
      }
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
  // unambigious. If there is one and only one entry, and zero-click is
  // enabled for that entry, return it.
  //
  // Moreover, we only return such a credential if the user has opted-in via the
  // first-run experience.
  bool can_use_autosignin = local_results.size() == 1u &&
                            !local_results[0]->skip_zero_click &&
                            delegate_->IsZeroClickAllowed();
  if (can_use_autosignin &&
      !password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          delegate_->client()->GetPrefs())) {
    CredentialInfo info(*local_results[0],
                        local_results[0]->federation_url.is_empty()
                            ? CredentialType::CREDENTIAL_TYPE_PASSWORD
                            : CredentialType::CREDENTIAL_TYPE_FEDERATED);
    delegate_->client()->NotifyUserAutoSignin(std::move(local_results));
    delegate_->SendCredential(id_, info);
    return;
  }

  // If we didn't even try to ask the user for credentials because the site
  // asked us for zero-click only and we're blocked on the first-run experience,
  // then notify the client.
  if (zero_click_only_ && can_use_autosignin &&
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          delegate_->client()->GetPrefs())) {
    scoped_ptr<autofill::PasswordForm> form(
        new autofill::PasswordForm(*local_results[0]));
    delegate_->client()->NotifyUserAutoSigninBlockedOnFirstRun(std::move(form));
  }

  // Otherwise, return an empty credential if we're in zero-click-only mode
  // or if the user chooses not to return a credential, and the credential the
  // user chooses if they pick one.
  if (zero_click_only_ ||
      !delegate_->client()->PromptUserToChooseCredentials(
          std::move(local_results), std::move(federated_results), origin_,
          base::Bind(
              &CredentialManagerPendingRequestTaskDelegate::SendCredential,
              base::Unretained(delegate_), id_))) {
    delegate_->SendCredential(id_, CredentialInfo());
  }
}

}  // namespace password_manager
