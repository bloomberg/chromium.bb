// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/sync/browser/sync_store_result_filter.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "components/password_manager/sync/browser/password_sync_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"

using autofill::PasswordForm;

namespace password_manager {

namespace {

// Returns true if the last loaded page was for transactional re-auth on a
// Google property.
bool LastLoadWasTransactionalReauthPage(const GURL& last_load_url) {
  if (last_load_url.GetOrigin() !=
      GaiaUrls::GetInstance()->gaia_url().GetOrigin())
    return false;

  // TODO(vabr): GAIA stops using the "rart" URL param, and instead includes a
  // hidden form field with name "rart". http://crbug.com/543085
  // "rart" is the transactional reauth paramter.
  std::string ignored_value;
  return net::GetValueForKeyInQuery(last_load_url, "rart", &ignored_value);
}

}  // namespace

SyncStoreResultFilter::SyncStoreResultFilter(
    const PasswordManagerClient* client,
    SyncServiceFactoryFunction sync_service_factory_function,
    SigninManagerFactoryFunction signin_manager_factory_function)
    : client_(client),
      sync_service_factory_function_(sync_service_factory_function),
      signin_manager_factory_function_(signin_manager_factory_function) {}

SyncStoreResultFilter::~SyncStoreResultFilter() {
}

ScopedVector<PasswordForm> SyncStoreResultFilter::FilterResults(
    ScopedVector<PasswordForm> results) const {
  const AutofillForSyncCredentialsState autofill_sync_state =
      GetAutofillForSyncCredentialsState();

  if (autofill_sync_state != DISALLOW_SYNC_CREDENTIALS &&
      (autofill_sync_state != DISALLOW_SYNC_CREDENTIALS_FOR_REAUTH ||
       !LastLoadWasTransactionalReauthPage(
           client_->GetLastCommittedEntryURL()))) {
    return results.Pass();
  }

  auto begin_of_removed =
      std::partition(results.begin(), results.end(),
                     [this](PasswordForm* form) { return ShouldSave(*form); });

  // TODO(vabr): Improve the description of the histogram to mention that it is
  // only reported for forms where the sync credentials would have been filled
  // in.
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.SyncCredentialFiltered",
                        begin_of_removed != results.end());

  results.erase(begin_of_removed, results.end());

  return results.Pass();
}

bool SyncStoreResultFilter::ShouldSave(
    const autofill::PasswordForm& form) const {
  return !sync_util::IsSyncAccountCredential(
      form, sync_service_factory_function_.Run(),
      signin_manager_factory_function_.Run());
}

void SyncStoreResultFilter::ReportFormUsed(
    const autofill::PasswordForm& form) const {
  base::RecordAction(
      base::UserMetricsAction("PasswordManager_SyncCredentialUsed"));
}

// static
SyncStoreResultFilter::AutofillForSyncCredentialsState
SyncStoreResultFilter::GetAutofillForSyncCredentialsState() {
  std::string group_name =
      base::FieldTrialList::FindFullName("AutofillSyncCredential");

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAllowAutofillSyncCredential))
    return ALLOW_SYNC_CREDENTIALS;
  if (command_line->HasSwitch(
          switches::kDisallowAutofillSyncCredentialForReauth)) {
    return DISALLOW_SYNC_CREDENTIALS_FOR_REAUTH;
  }
  if (command_line->HasSwitch(switches::kDisallowAutofillSyncCredential))
    return DISALLOW_SYNC_CREDENTIALS;

  if (group_name == "DisallowSyncCredentialsForReauth")
    return DISALLOW_SYNC_CREDENTIALS_FOR_REAUTH;
  if (group_name == "DisallowSyncCredentials")
    return DISALLOW_SYNC_CREDENTIALS;

  // Allow by default.
  return ALLOW_SYNC_CREDENTIALS;
}

}  // namespace password_manager
