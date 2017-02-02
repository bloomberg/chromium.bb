// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_fetcher_impl.h"

#include <algorithm>
#include <utility>

#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/statistics_table.h"

using autofill::PasswordForm;

// Shorten the name to spare line breaks. The code provides enough context
// already.
using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

namespace {

// Splits |store_results| into a vector of non-federated and federated matches.
// Returns the federated matches and keeps the non-federated in |store_results|.
std::vector<std::unique_ptr<PasswordForm>> SplitFederatedMatches(
    std::vector<std::unique_ptr<PasswordForm>>* store_results) {
  const auto first_federated = std::partition(
      store_results->begin(), store_results->end(),
      [](const std::unique_ptr<PasswordForm>& form) {
        return form->federation_origin.unique();  // False means federated.
      });

  // Move out federated matches.
  std::vector<std::unique_ptr<PasswordForm>> federated_matches;
  federated_matches.resize(store_results->end() - first_federated);
  std::move(first_federated, store_results->end(), federated_matches.begin());

  store_results->erase(first_federated, store_results->end());
  return federated_matches;
}

// Create a vector of const PasswordForm from a vector of
// unique_ptr<PasswordForm> by applying get() item-wise.
std::vector<const PasswordForm*> MakeWeakCopies(
    const std::vector<std::unique_ptr<PasswordForm>>& owning) {
  std::vector<const PasswordForm*> result(owning.size());
  std::transform(
      owning.begin(), owning.end(), result.begin(),
      [](const std::unique_ptr<PasswordForm>& ptr) { return ptr.get(); });
  return result;
}

}  // namespace

FormFetcherImpl::FormFetcherImpl(PasswordStore::FormDigest form_digest,
                                 const PasswordManagerClient* client,
                                 bool should_migrate_http_passwords)
    : form_digest_(std::move(form_digest)),
      client_(client),
      should_migrate_http_passwords_(should_migrate_http_passwords) {}

FormFetcherImpl::~FormFetcherImpl() = default;

void FormFetcherImpl::AddConsumer(FormFetcher::Consumer* consumer) {
  DCHECK(consumer);
  consumers_.insert(consumer);
  if (state_ == State::NOT_WAITING)
    consumer->ProcessMatches(weak_non_federated_, filtered_count_);
}

FormFetcherImpl::State FormFetcherImpl::GetState() const {
  return state_;
}

const std::vector<InteractionsStats>& FormFetcherImpl::GetInteractionsStats()
    const {
  return interactions_stats_;
}

const std::vector<const PasswordForm*>& FormFetcherImpl::GetFederatedMatches()
    const {
  return weak_federated_;
}

void FormFetcherImpl::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  DCHECK_EQ(State::WAITING, state_);
  state_ = State::NOT_WAITING;

  if (need_to_refetch_) {
    // The received results are no longer up to date, need to re-request.
    Fetch();
    need_to_refetch_ = false;
    return;
  }

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_ON_GET_STORE_RESULTS_METHOD);
    logger->LogNumber(Logger::STRING_NUMBER_RESULTS, results.size());
  }

  if (should_migrate_http_passwords_ && results.empty() &&
      form_digest_.origin.SchemeIs(url::kHttpsScheme)) {
    http_migrator_ = base::MakeUnique<HttpPasswordMigrator>(
        form_digest_.origin, HttpPasswordMigrator::MigrationMode::COPY,
        client_->GetPasswordStore(), this);
    return;
  }

  ProcessPasswordStoreResults(std::move(results));
}

void FormFetcherImpl::OnGetSiteStatistics(
    std::vector<InteractionsStats> stats) {
  // On Windows the password request may be resolved after the statistics due to
  // importing from IE.
  interactions_stats_ = std::move(stats);
}

void FormFetcherImpl::ProcessMigratedForms(
    std::vector<std::unique_ptr<autofill::PasswordForm>> forms) {
  ProcessPasswordStoreResults(std::move(forms));
}

void FormFetcherImpl::Fetch() {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_FETCH_METHOD);
    logger->LogNumber(Logger::STRING_FORM_FETCHER_STATE,
                      static_cast<int>(state_));
  }

  if (state_ == State::WAITING) {
    // There is currently a password store query in progress, need to re-fetch
    // store results later.
    need_to_refetch_ = true;
    return;
  }

  PasswordStore* password_store = client_->GetPasswordStore();
  if (!password_store) {
    if (logger)
      logger->LogMessage(Logger::STRING_NO_STORE);
    NOTREACHED();
    return;
  }
  state_ = State::WAITING;
  password_store->GetLogins(form_digest_, this);

// The statistics isn't needed on mobile, only on desktop. Let's save some
// processor cycles.
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  // The statistics is needed for the "Save password?" bubble.
  password_store->GetSiteStats(form_digest_.origin.GetOrigin(), this);
#endif
}

void FormFetcherImpl::ProcessPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  federated_ = SplitFederatedMatches(&results);
  non_federated_ = std::move(results);

  const size_t original_count = non_federated_.size();

  non_federated_ =
      client_->GetStoreResultFilter()->FilterResults(std::move(non_federated_));

  filtered_count_ = original_count - non_federated_.size();

  weak_non_federated_ = MakeWeakCopies(non_federated_);
  weak_federated_ = MakeWeakCopies(federated_);

  for (FormFetcher::Consumer* consumer : consumers_)
    consumer->ProcessMatches(weak_non_federated_, filtered_count_);
}

}  // namespace password_manager
