// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/obsolete_http_cleaner.h"

#include <algorithm>
#include <iterator>
#include <tuple>

#include "base/logging.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "url/url_constants.h"

using autofill::PasswordForm;

namespace password_manager {

namespace {

std::vector<std::unique_ptr<PasswordForm>> SplitFormsFrom(
    std::vector<std::unique_ptr<PasswordForm>>::iterator from,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  result.reserve(std::distance(from, std::end(*forms)));
  std::move(from, std::end(*forms), std::back_inserter(result));
  forms->erase(from, std::end(*forms));
  return result;
}

}  // namespace

ObsoleteHttpCleaner::ObsoleteHttpCleaner(const PasswordManagerClient* client)
    : client_(client) {
  DCHECK(client_);
}

ObsoleteHttpCleaner::~ObsoleteHttpCleaner() = default;

void ObsoleteHttpCleaner::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // Non HTTP or HTTPS credentials are ignored.
  results.erase(std::remove_if(std::begin(results), std::end(results),
                               [](const std::unique_ptr<PasswordForm>& form) {
                                 return !form->origin.SchemeIsHTTPOrHTTPS();
                               }),
                std::end(results));

  // Move HTTPS forms into their own container.
  auto https_forms = SplitFormsFrom(
      std::partition(std::begin(results), std::end(results),
                     [](const std::unique_ptr<PasswordForm>& form) {
                       return form->origin.SchemeIs(url::kHttpScheme);
                     }),
      &results);

  // Move blacklisted HTTP forms into their own container.
  const auto blacklisted_http_forms = SplitFormsFrom(
      std::partition(std::begin(results), std::end(results),
                     [](const std::unique_ptr<PasswordForm>& form) {
                       return !form->blacklisted_by_user;
                     }),
      &results);

  // Remove blacklisted HTTP forms from the password store when HSTS is active
  // for the given host.
  for (const auto& form : blacklisted_http_forms) {
    if (client_->IsHSTSActiveForHost(form->origin))
      client_->GetPasswordStore()->RemoveLogin(*form);
  }

  // Return early if there are no non-blacklisted HTTP forms.
  if (results.empty())
    return;

  // Ignore non HSTS forms.
  https_forms.erase(
      std::remove_if(std::begin(https_forms), std::end(https_forms),
                     [this](const std::unique_ptr<PasswordForm>& form) {
                       return !client_->IsHSTSActiveForHost(form->origin);
                     }),
      std::end(https_forms));

  // Sort HSTS forms according to custom comparison function. Consider two forms
  // equivalent if they have the same host, as well as the same username and
  // password.
  const auto form_cmp = [](const std::unique_ptr<PasswordForm>& lhs,
                           const std::unique_ptr<PasswordForm>& rhs) {
    return std::forward_as_tuple(lhs->origin.host_piece(), lhs->username_value,
                                 lhs->password_value) <
           std::forward_as_tuple(rhs->origin.host_piece(), rhs->username_value,
                                 rhs->password_value);
  };

  std::sort(std::begin(https_forms), std::end(https_forms), form_cmp);

  // Iterate through HTTP forms and remove them from the password store if there
  // exists an equivalent HSTS form.
  for (const auto& form : results) {
    if (std::binary_search(std::begin(https_forms), std::end(https_forms), form,
                           form_cmp))
      client_->GetPasswordStore()->RemoveLogin(*form);
  }
}

void ObsoleteHttpCleaner::OnGetSiteStatistics(
    std::vector<InteractionsStats> stats) {
  for (const auto& stat : stats) {
    if (stat.origin_domain.SchemeIs(url::kHttpScheme) &&
        client_->IsHSTSActiveForHost(stat.origin_domain))
      client_->GetPasswordStore()->RemoveSiteStats(stat.origin_domain);
  }
}

}  // namespace password_manager
