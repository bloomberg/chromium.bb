// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/obsolete_http_cleaner.h"

#include <algorithm>
#include <iterator>
#include <tuple>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
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

void RemoveLoginIfHSTS(const scoped_refptr<PasswordStore>& store,
                       const PasswordForm& form,
                       bool is_hsts) {
  if (is_hsts)
    store->RemoveLogin(form);
}

void RemoveSiteStatsIfHSTS(const scoped_refptr<PasswordStore>& store,
                           const InteractionsStats& stats,
                           bool is_hsts) {
  if (is_hsts)
    store->RemoveSiteStats(stats.origin_domain);
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
  base::EraseIf(results, [](const std::unique_ptr<PasswordForm>& form) {
    return !form->origin.SchemeIsHTTPOrHTTPS();
  });

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
    client_->PostHSTSQueryForHost(
        form->origin,
        base::Bind(RemoveLoginIfHSTS,
                   make_scoped_refptr(client_->GetPasswordStore()), *form));
  }

  // Return early if there are no non-blacklisted HTTP forms.
  if (results.empty())
    return;

  // Sort HTTPS forms according to custom comparison function. Consider two
  // forms equivalent if they have the same host, as well as the same username
  // and password.
  const auto form_cmp = [](const std::unique_ptr<PasswordForm>& lhs,
                           const std::unique_ptr<PasswordForm>& rhs) {
    return std::forward_as_tuple(lhs->origin.host_piece(), lhs->username_value,
                                 lhs->password_value) <
           std::forward_as_tuple(rhs->origin.host_piece(), rhs->username_value,
                                 rhs->password_value);
  };

  std::sort(std::begin(https_forms), std::end(https_forms), form_cmp);

  // Iterate through HTTP forms and remove them from the password store if there
  // exists an equivalent HTTPS form that has HSTS enabled.
  for (const auto& form : results) {
    if (std::binary_search(std::begin(https_forms), std::end(https_forms), form,
                           form_cmp)) {
      client_->PostHSTSQueryForHost(
          form->origin,
          base::Bind(RemoveLoginIfHSTS,
                     make_scoped_refptr(client_->GetPasswordStore()), *form));
    }
  }
}

void ObsoleteHttpCleaner::OnGetSiteStatistics(
    std::vector<InteractionsStats> stats) {
  for (const auto& stat : stats) {
    if (stat.origin_domain.SchemeIs(url::kHttpScheme)) {
      client_->PostHSTSQueryForHost(
          stat.origin_domain,
          base::Bind(RemoveSiteStatsIfHSTS,
                     make_scoped_refptr(client_->GetPasswordStore()), stat));
    }
  }
}

}  // namespace password_manager
