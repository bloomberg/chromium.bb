// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/suppressed_https_form_fetcher.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_store.h"
#include "url/gurl.h"

namespace password_manager {

SuppressedHTTPSFormFetcher::SuppressedHTTPSFormFetcher(
    const std::string& observed_signon_realm,
    const PasswordManagerClient* client,
    Consumer* consumer)
    : client_(client),
      consumer_(consumer),
      observed_signon_realm_as_url_(observed_signon_realm) {
  DCHECK(client_);
  DCHECK(consumer_);
  DCHECK(observed_signon_realm_as_url_.SchemeIs(url::kHttpScheme));
  client_->GetPasswordStore()->GetLoginsForSameOrganizationName(
      observed_signon_realm, this);
}

SuppressedHTTPSFormFetcher::~SuppressedHTTPSFormFetcher() = default;

void SuppressedHTTPSFormFetcher::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  base::EraseIf(
      results, [this](const std::unique_ptr<autofill::PasswordForm>& form) {
        GURL candidate_signon_realm_as_url(form->signon_realm);
        return !candidate_signon_realm_as_url.SchemeIs(url::kHttpsScheme) ||
               candidate_signon_realm_as_url.host() !=
                   observed_signon_realm_as_url_.host();
      });

  consumer_->ProcessSuppressedHTTPSForms(std::move(results));
}

}  // namespace password_manager
