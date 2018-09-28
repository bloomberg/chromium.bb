// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/http_credentials_cleaner.h"

#include "base/metrics/histogram_functions.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "url/gurl.h"

namespace password_manager {

HttpCredentialCleaner::HttpCredentialCleaner(
    scoped_refptr<PasswordStore> store,
    base::RepeatingCallback<network::mojom::NetworkContext*()>
        network_context_getter)
    : store_(std::move(store)),
      network_context_getter_(network_context_getter) {
}

HttpCredentialCleaner::~HttpCredentialCleaner() = default;

void HttpCredentialCleaner::StartCleaning(Observer* observer) {
  DCHECK(observer);
  DCHECK(!observer_);
  observer_ = observer;
  store_->GetAutofillableLogins(this);
}

void HttpCredentialCleaner::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  // Non HTTP or HTTPS credentials are ignored.
  base::EraseIf(results, [](const auto& form) {
    return !form->origin.SchemeIsHTTPOrHTTPS();
  });

  for (auto& form : results) {
    FormKey form_key(
        {std::string(
             password_manager_util::GetSignonRealmWithProtocolExcluded(*form)),
         form->scheme, form->username_value});
    if (form->origin.SchemeIs(url::kHttpScheme)) {
      PostHSTSQueryForHostAndNetworkContext(
          form->origin, network_context_getter_.Run(),
          base::Bind(&HttpCredentialCleaner::OnHSTSQueryResult,
                     base::Unretained(this), form_key, form->password_value));
      ++total_http_credentials_;
    } else {  // HTTPS
      https_credentials_map_[form_key].insert(form->password_value);
    }
  }

  // This is needed in case of empty |results|.
  ReportMetrics();
}

// |key| and |password_value| was created from the same form.
void HttpCredentialCleaner::OnHSTSQueryResult(
    FormKey key,
    base::string16 password_value,
    password_manager::HSTSResult hsts_result) {
  ++processed_results_;
  base::ScopedClosureRunner report(base::BindOnce(
      &HttpCredentialCleaner::ReportMetrics, base::Unretained(this)));

  if (hsts_result == HSTSResult::kError)
    return;

  bool is_hsts = (hsts_result == HSTSResult::kYes);

  auto user_it = https_credentials_map_.find(key);
  if (user_it == https_credentials_map_.end()) {
    // Credentials are not migrated yet.
    ++https_credential_not_found_[is_hsts];
    return;
  }

  if (base::ContainsKey(user_it->second, password_value)) {
    // The password store contains the same credentials (signon_realm, scheme,
    // username and password) on HTTP version of the form.
    ++same_password_[is_hsts];
  } else {
    ++different_password_[is_hsts];
  }
}

void HttpCredentialCleaner::ReportMetrics() {
  // The metrics have to be recorded after all requests are done.
  if (processed_results_ != total_http_credentials_)
    return;

  for (bool is_hsts_enabled : {false, true}) {
    std::string suffix = (is_hsts_enabled ? std::string("WithHSTSEnabled")
                                          : std::string("HSTSNotEnabled"));

    base::UmaHistogramCounts1000(
        "PasswordManager.HttpCredentialsWithEquivalentHttpsCredential." +
            suffix,
        same_password_[is_hsts_enabled]);

    base::UmaHistogramCounts1000(
        "PasswordManager.HttpCredentialsWithConflictingHttpsCredential." +
            suffix,
        different_password_[is_hsts_enabled]);

    base::UmaHistogramCounts1000(
        "PasswordManager.HttpCredentialsWithoutMatchingHttpsCredential." +
            suffix,
        https_credential_not_found_[is_hsts_enabled]);
  }

  observer_->CleaningCompleted();
}

}  // namespace password_manager