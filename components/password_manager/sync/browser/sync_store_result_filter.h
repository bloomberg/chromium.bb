// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_SYNC_BROWSER_STORE_RESULT_FILTER_H_
#define COMPONENTS_PASSWORD_MANAGER_SYNC_BROWSER_STORE_RESULT_FILTER_H_

#include "components/password_manager/core/browser/store_result_filter.h"

namespace password_manager {

class PasswordManagerClient;

// The sync- and GAIA- aware implementation of the filter.
class SyncStoreResultFilter : public StoreResultFilter {
 public:
  explicit SyncStoreResultFilter(const PasswordManagerClient* client);
  ~SyncStoreResultFilter() override;

  // StoreResultFilter
  bool ShouldIgnore(const autofill::PasswordForm& form) override;

 private:
  enum AutofillForSyncCredentialsState {
    ALLOW_SYNC_CREDENTIALS,
    DISALLOW_SYNC_CREDENTIALS_FOR_REAUTH,
    DISALLOW_SYNC_CREDENTIALS,
  };

  // Determines autofill state based on experiment and flag values.
  static AutofillForSyncCredentialsState SetUpAutofillSyncState();

  const PasswordManagerClient* const client_;

  // How to handle the sync credential in ShouldFilterAutofillResult().
  const AutofillForSyncCredentialsState autofill_sync_state_;

  // For statistics about filtering the sync credential during autofill.
  bool sync_credential_was_filtered_;

  DISALLOW_COPY_AND_ASSIGN(SyncStoreResultFilter);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_SYNC_BROWSER_STORE_RESULT_FILTER_H_
