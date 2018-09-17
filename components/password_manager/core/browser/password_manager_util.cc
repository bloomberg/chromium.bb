// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/blacklisted_duplicates_cleaner.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "components/password_manager/core/browser/credentials_cleaner_runner.h"
#include "components/password_manager/core/browser/hsts_query.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "url/gurl.h"

using autofill::PasswordForm;

namespace password_manager_util {
namespace {

// Return true if
// 1.|lhs| is non-PSL match, |rhs| is PSL match or
// 2.|lhs| and |rhs| have the same value of |is_public_suffix_match|, and |lhs|
// is preferred while |rhs| is not preferred.
bool IsBetterMatch(const PasswordForm* lhs, const PasswordForm* rhs) {
  return std::make_pair(!lhs->is_public_suffix_match, lhs->preferred) >
         std::make_pair(!rhs->is_public_suffix_match, rhs->preferred);
}

// This class is responsible for reporting metrics about HTTP to HTTPS
// migration.
class HttpMetricsMigrationReporter
    : public password_manager::PasswordStoreConsumer {
 public:
  HttpMetricsMigrationReporter(
      password_manager::PasswordStore* store,
      base::RepeatingCallback<network::mojom::NetworkContext*()>
          network_context_getter)
      : network_context_getter_(network_context_getter) {
    store->GetAutofillableLogins(this);
  }

 private:
  // This type define a subset of PasswordForm where first argument is the
  // signon-realm excluding the protocol, the second argument is
  // PasswordForm::scheme (i.e. HTML, BASIC, etc.) and the third argument is the
  // username of the form.
  using FormKey = std::tuple<std::string, PasswordForm::Scheme, base::string16>;

  // This overrides the PasswordStoreConsumer method.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  void OnHSTSQueryResult(FormKey key,
                         base::string16 password_value,
                         password_manager::HSTSResult is_hsts);

  void ReportMetrics();

  base::RepeatingCallback<network::mojom::NetworkContext*()>
      network_context_getter_;

  std::map<FormKey, base::flat_set<base::string16>> https_credentials_map_;
  size_t processed_results_ = 0;

  // The next three counters are in pairs where [0] component means that HSTS is
  // not enabled and [1] component means that HSTS is enabled for that HTTP type
  // of credentials.

  // Number of HTTP credentials for which no HTTPS credential for the same
  // username exists.
  size_t https_credential_not_found_[2] = {0, 0};

  // Number of HTTP credentials for which an equivalent (i.e. same host,
  // username and password) HTTPS credential exists.
  size_t same_password_[2] = {0, 0};

  // Number of HTTP credentials for which a conflicting (i.e. same host and
  // username, but different password) HTTPS credential exists.
  size_t different_password_[2] = {0, 0};

  // Number of HTTP credentials from the Password Store.
  size_t total_http_credentials_ = 0;

  DISALLOW_COPY_AND_ASSIGN(HttpMetricsMigrationReporter);
};

void HttpMetricsMigrationReporter::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  // Non HTTP or HTTPS credentials are ignored.
  base::EraseIf(results, [](const std::unique_ptr<PasswordForm>& form) {
    return !form->origin.SchemeIsHTTPOrHTTPS();
  });

  for (auto& form : results) {
    // The next signon-realm has the protocol excluded. For example if original
    // signon_realm is "https://google.com/". After excluding protocol it
    // becomes "google.com/".
    FormKey form_key({GURL(form->signon_realm).GetContent(), form->scheme,
                      form->username_value});
    if (form->origin.SchemeIs(url::kHttpScheme)) {
      password_manager::PostHSTSQueryForHostAndNetworkContext(
          form->origin, network_context_getter_.Run(),
          base::Bind(&HttpMetricsMigrationReporter::OnHSTSQueryResult,
                     base::Unretained(this), form_key, form->password_value));
      ++total_http_credentials_;
    } else {  // Https
      https_credentials_map_[form_key].insert(form->password_value);
    }
  }
  ReportMetrics();
}

// |key| and |password_value| was created from the same form.
void HttpMetricsMigrationReporter::OnHSTSQueryResult(
    FormKey key,
    base::string16 password_value,
    password_manager::HSTSResult hsts_result) {
  ++processed_results_;
  base::ScopedClosureRunner report(base::BindOnce(
      &HttpMetricsMigrationReporter::ReportMetrics, base::Unretained(this)));

  if (hsts_result == password_manager::HSTSResult::kError)
    return;

  bool is_hsts = (hsts_result == password_manager::HSTSResult::kYes);

  auto user_it = https_credentials_map_.find(key);
  if (user_it == https_credentials_map_.end()) {
    // Credentials are not migrated yet.
    ++https_credential_not_found_[is_hsts];
    return;
  }
  if (base::ContainsKey(user_it->second, password_value)) {
    // The password store contains the same credentials (username and
    // password) on HTTP version of the form.
    ++same_password_[is_hsts];
  } else {
    ++different_password_[is_hsts];
  }
}

void HttpMetricsMigrationReporter::ReportMetrics() {
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
  delete this;
}

}  // namespace

#if !defined(OS_IOS)
void ReportHttpMigrationMetrics(
    scoped_refptr<password_manager::PasswordStore> store,
    base::RepeatingCallback<network::mojom::NetworkContext*()>
        network_context_getter) {
  // The object will delete itself once the metrics are recorded.
  new HttpMetricsMigrationReporter(store.get(), network_context_getter);
}
#endif  // !defined(OS_IOS)

// Update |credential| to reflect usage.
void UpdateMetadataForUsage(PasswordForm* credential) {
  ++credential->times_used;

  // Remove alternate usernames. At this point we assume that we have found
  // the right username.
  credential->other_possible_usernames.clear();
}

password_manager::SyncState GetPasswordSyncState(
    const syncer::SyncService* sync_service) {
  if (sync_service && sync_service->IsFirstSetupComplete() &&
      sync_service->IsSyncFeatureActive() &&
      sync_service->GetActiveDataTypes().Has(syncer::PASSWORDS)) {
    return sync_service->IsUsingSecondaryPassphrase()
               ? password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE
               : password_manager::SYNCING_NORMAL_ENCRYPTION;
  }
  return password_manager::NOT_SYNCING;
}

password_manager::SyncState GetHistorySyncState(
    const syncer::SyncService* sync_service) {
  if (sync_service && sync_service->IsFirstSetupComplete() &&
      sync_service->IsSyncFeatureActive() &&
      (sync_service->GetActiveDataTypes().Has(
           syncer::HISTORY_DELETE_DIRECTIVES) ||
       sync_service->GetActiveDataTypes().Has(syncer::PROXY_TABS))) {
    return sync_service->IsUsingSecondaryPassphrase()
               ? password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE
               : password_manager::SYNCING_NORMAL_ENCRYPTION;
  }
  return password_manager::NOT_SYNCING;
}

void FindDuplicates(std::vector<std::unique_ptr<PasswordForm>>* forms,
                    std::vector<std::unique_ptr<PasswordForm>>* duplicates,
                    std::vector<std::vector<PasswordForm*>>* tag_groups) {
  if (forms->empty())
    return;

  // Linux backends used to treat the first form as a prime oneamong the
  // duplicates. Therefore, the caller should try to preserve it.
  std::stable_sort(forms->begin(), forms->end(), autofill::LessThanUniqueKey());

  std::vector<std::unique_ptr<PasswordForm>> unique_forms;
  unique_forms.push_back(std::move(forms->front()));
  if (tag_groups) {
    tag_groups->clear();
    tag_groups->push_back(std::vector<PasswordForm*>());
    tag_groups->front().push_back(unique_forms.front().get());
  }
  for (auto it = forms->begin() + 1; it != forms->end(); ++it) {
    if (ArePasswordFormUniqueKeyEqual(**it, *unique_forms.back())) {
      if (tag_groups)
        tag_groups->back().push_back(it->get());
      duplicates->push_back(std::move(*it));
    } else {
      if (tag_groups)
        tag_groups->push_back(std::vector<PasswordForm*>(1, it->get()));
      unique_forms.push_back(std::move(*it));
    }
  }
  forms->swap(unique_forms);
}

void TrimUsernameOnlyCredentials(
    std::vector<std::unique_ptr<PasswordForm>>* android_credentials) {
  // Remove username-only credentials which are not federated.
  base::EraseIf(*android_credentials,
                [](const std::unique_ptr<PasswordForm>& form) {
                  return form->scheme == PasswordForm::SCHEME_USERNAME_ONLY &&
                         form->federation_origin.unique();
                });

  // Set "skip_zero_click" on federated credentials.
  std::for_each(android_credentials->begin(), android_credentials->end(),
                [](const std::unique_ptr<PasswordForm>& form) {
                  if (form->scheme == PasswordForm::SCHEME_USERNAME_ONLY)
                    form->skip_zero_click = true;
                });
}

bool IsLoggingActive(const password_manager::PasswordManagerClient* client) {
  const password_manager::LogManager* log_manager = client->GetLogManager();
  return log_manager && log_manager->IsLoggingActive();
}

bool ManualPasswordGenerationEnabled(
    password_manager::PasswordManagerDriver* driver) {
  password_manager::PasswordGenerationManager* password_generation_manager =
      driver ? driver->GetPasswordGenerationManager() : nullptr;
  if (!password_generation_manager ||
      !password_generation_manager->IsGenerationEnabled(false /*logging*/)) {
    return false;
  }

  LogPasswordGenerationEvent(
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_SHOWN);
  return true;
}

bool ShowAllSavedPasswordsContextMenuEnabled(
    password_manager::PasswordManagerDriver* driver) {
  password_manager::PasswordManager* password_manager =
      driver ? driver->GetPasswordManager() : nullptr;
  if (!password_manager)
    return false;

  password_manager::PasswordManagerClient* client = password_manager->client();
  if (!client || !client->IsFillingFallbackEnabledForCurrentPage())
    return false;

  LogContextOfShowAllSavedPasswordsShown(
      password_manager::metrics_util::
          SHOW_ALL_SAVED_PASSWORDS_CONTEXT_CONTEXT_MENU);

  return true;
}

void UserTriggeredShowAllSavedPasswordsFromContextMenu(
    autofill::AutofillClient* autofill_client) {
  if (!autofill_client)
    return;
  autofill_client->ExecuteCommand(
      autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY);
  password_manager::metrics_util::LogContextOfShowAllSavedPasswordsAccepted(
      password_manager::metrics_util::
          SHOW_ALL_SAVED_PASSWORDS_CONTEXT_CONTEXT_MENU);
}

void UserTriggeredManualGenerationFromContextMenu(
    password_manager::PasswordManagerClient* password_manager_client) {
  password_manager_client->GeneratePassword();
  LogPasswordGenerationEvent(
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_PRESSED);
}

void DeleteBlacklistedDuplicates(
    scoped_refptr<password_manager::PasswordStore> store,
    PrefService* prefs,
    int delay_in_seconds) {
  const bool need_to_remove_blacklisted_duplicates = !prefs->GetBoolean(
      password_manager::prefs::kDuplicatedBlacklistedCredentialsRemoved);
  base::UmaHistogramBoolean(
      "PasswordManager.BlacklistedSites.NeedRemoveBlacklistDuplicates",
      need_to_remove_blacklisted_duplicates);

  // The object will delete itself once the clearing tasks are done.
  auto* cleaning_tasks_runner =
      new password_manager::CredentialsCleanerRunner();

  if (need_to_remove_blacklisted_duplicates) {
    cleaning_tasks_runner->AddCleaningTask(
        std::make_unique<password_manager::BlacklistedDuplicatesCleaner>(
            store, prefs));
  }

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&password_manager::CredentialsCleanerRunner::StartCleaning,
                     base::Unretained(cleaning_tasks_runner)),
      base::TimeDelta::FromSeconds(delay_in_seconds));
}

void FindBestMatches(
    std::vector<const PasswordForm*> matches,
    std::map<base::string16, const PasswordForm*>* best_matches,
    std::vector<const PasswordForm*>* not_best_matches,
    const PasswordForm** preferred_match) {
  DCHECK(std::all_of(
      matches.begin(), matches.end(),
      [](const PasswordForm* match) { return !match->blacklisted_by_user; }));
  DCHECK(best_matches);
  DCHECK(not_best_matches);
  DCHECK(preferred_match);

  *preferred_match = nullptr;
  best_matches->clear();
  not_best_matches->clear();

  if (matches.empty())
    return;

  // Sort matches using IsBetterMatch predicate.
  std::sort(matches.begin(), matches.end(), IsBetterMatch);
  for (const auto* match : matches) {
    const base::string16& username = match->username_value;
    // The first match for |username| in the sorted array is best match.
    if (best_matches->find(username) == best_matches->end())
      best_matches->insert(std::make_pair(username, match));
    else
      not_best_matches->push_back(match);
  }

  *preferred_match = *matches.begin();
}

}  // namespace password_manager_util
