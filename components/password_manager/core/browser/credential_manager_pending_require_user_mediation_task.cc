// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_require_user_mediation_task.h"

#include "components/autofill/core/common/password_form.h"

namespace password_manager {

CredentialManagerPendingRequireUserMediationTask::
    CredentialManagerPendingRequireUserMediationTask(
        CredentialManagerPendingRequireUserMediationTaskDelegate* delegate)
    : delegate_(delegate), pending_requests_(0) {}

CredentialManagerPendingRequireUserMediationTask::
    ~CredentialManagerPendingRequireUserMediationTask() = default;

void CredentialManagerPendingRequireUserMediationTask::AddOrigin(
    const PasswordStore::FormDigest& form_digest) {
  delegate_->GetPasswordStore()->GetLogins(form_digest, this);
  pending_requests_++;
}

void CredentialManagerPendingRequireUserMediationTask::
    OnGetPasswordStoreResults(
        std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  PasswordStore* store = delegate_->GetPasswordStore();
  for (const auto& form : results) {
    if (!form->skip_zero_click) {
      form->skip_zero_click = true;
      store->UpdateLogin(*form);
    }
  }
  pending_requests_--;
  if (!pending_requests_)
    delegate_->DoneRequiringUserMediation();
}

}  // namespace password_manager
