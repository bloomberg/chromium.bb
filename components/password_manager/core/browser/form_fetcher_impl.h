// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_IMPL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/http_password_migrator.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace password_manager {

class PasswordManagerClient;

// Production implementation of FormFetcher. Fetches credentials associated
// with a particular origin.
class FormFetcherImpl : public FormFetcher,
                        public PasswordStoreConsumer,
                        public HttpPasswordMigrator::Consumer {
 public:
  // |form_digest| describes what credentials need to be retrieved and
  // |client| serves the PasswordStore, the logging information etc.
  FormFetcherImpl(PasswordStore::FormDigest form_digest,
                  const PasswordManagerClient* client,
                  bool should_migrate_http_passwords);

  ~FormFetcherImpl() override;

  // FormFetcher:
  void AddConsumer(FormFetcher::Consumer* consumer) override;
  State GetState() const override;
  const std::vector<InteractionsStats>& GetInteractionsStats() const override;
  const std::vector<const autofill::PasswordForm*>& GetFederatedMatches()
      const override;
  void Fetch() override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;
  void OnGetSiteStatistics(std::vector<InteractionsStats> stats) override;

  // HttpPasswordMigrator::Consumer:
  void ProcessMigratedForms(
      std::vector<std::unique_ptr<autofill::PasswordForm>> forms) override;

 private:
  // Processes password form results and forwards them to the |consumers_|.
  void ProcessPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results);

  // PasswordStore results will be fetched for this description.
  const PasswordStore::FormDigest form_digest_;

  // Results obtained from PasswordStore:
  std::vector<std::unique_ptr<autofill::PasswordForm>> non_federated_;

  // Federated credentials relevant to the observed form. They are neither
  // filled not saved by PasswordFormManager, so they are kept separately from
  // non-federated matches.
  std::vector<std::unique_ptr<autofill::PasswordForm>> federated_;

  // Statistics for the current domain.
  std::vector<InteractionsStats> interactions_stats_;

  // Non-owning copies of the vectors above.
  std::vector<const autofill::PasswordForm*> weak_non_federated_;
  std::vector<const autofill::PasswordForm*> weak_federated_;

  // Consumers of the fetcher, all are assumed to outlive |this|.
  std::set<FormFetcher::Consumer*> consumers_;

  // Client used to obtain a CredentialFilter.
  const PasswordManagerClient* const client_;

  // The number of non-federated forms which were filtered out by
  // CredentialsFilter and not included in |non_federated_|.
  size_t filtered_count_ = 0;

  // State of the fetcher.
  State state_ = State::NOT_WAITING;

  // False unless FetchDataFromPasswordStore has been called again without the
  // password store returning results in the meantime.
  bool need_to_refetch_ = false;

  // Indicates whether HTTP passwords should be migrated to HTTPS.
  const bool should_migrate_http_passwords_;

  // Does the actual migration.
  std::unique_ptr<HttpPasswordMigrator> http_migrator_;

  DISALLOW_COPY_AND_ASSIGN(FormFetcherImpl);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_FETCHER_IMPL_H_
