// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_SYNC_BROWSER_STORE_RESULT_FILTER_H_
#define COMPONENTS_PASSWORD_MANAGER_SYNC_BROWSER_STORE_RESULT_FILTER_H_

#include "components/password_manager/core/browser/credentials_filter.h"

namespace password_manager {

class PasswordManagerClient;

// The sync- and GAIA- aware implementation of the filter.
// TODO(vabr): Rename this to match the interface.
class SyncStoreResultFilter : public CredentialsFilter {
 public:
  explicit SyncStoreResultFilter(const PasswordManagerClient* client);
  ~SyncStoreResultFilter() override;

  // CredentialsFilter
  ScopedVector<autofill::PasswordForm> FilterResults(
      ScopedVector<autofill::PasswordForm> results) const override;

 private:
  enum AutofillForSyncCredentialsState {
    ALLOW_SYNC_CREDENTIALS,
    DISALLOW_SYNC_CREDENTIALS_FOR_REAUTH,
    DISALLOW_SYNC_CREDENTIALS,
  };

  // Determines autofill state based on experiment and flag values.
  static AutofillForSyncCredentialsState GetAutofillForSyncCredentialsState();

  const PasswordManagerClient* const client_;

  DISALLOW_COPY_AND_ASSIGN(SyncStoreResultFilter);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_SYNC_BROWSER_STORE_RESULT_FILTER_H_
