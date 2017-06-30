// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_

#include <string>
#include <vector>

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "testing/gmock/include/gmock/gmock.h"

class PrefService;

namespace password_manager {

class MockPasswordStore : public PasswordStore {
 public:
  MockPasswordStore();

  bool Init(const syncer::SyncableService::StartSyncFlare& flare,
            PrefService* prefs) override {
    return true;
  };
  MOCK_METHOD1(RemoveLogin, void(const autofill::PasswordForm&));
  MOCK_METHOD2(GetLogins,
               void(const PasswordStore::FormDigest&, PasswordStoreConsumer*));
  MOCK_METHOD2(GetLoginsForSameOrganizationName,
               void(const std::string&, PasswordStoreConsumer*));
  MOCK_METHOD1(AddLogin, void(const autofill::PasswordForm&));
  MOCK_METHOD1(UpdateLogin, void(const autofill::PasswordForm&));
  MOCK_METHOD2(UpdateLoginWithPrimaryKey,
               void(const autofill::PasswordForm&,
                    const autofill::PasswordForm&));
  MOCK_METHOD2(ReportMetrics, void(const std::string&, bool));
  MOCK_METHOD2(ReportMetricsImpl, void(const std::string&, bool));
  MOCK_METHOD1(AddLoginImpl,
               PasswordStoreChangeList(const autofill::PasswordForm&));
  MOCK_METHOD1(UpdateLoginImpl,
               PasswordStoreChangeList(const autofill::PasswordForm&));
  MOCK_METHOD1(RemoveLoginImpl,
               PasswordStoreChangeList(const autofill::PasswordForm&));
  MOCK_METHOD3(RemoveLoginsByURLAndTimeImpl,
               PasswordStoreChangeList(const base::Callback<bool(const GURL&)>&,
                                       base::Time,
                                       base::Time));
  MOCK_METHOD2(RemoveLoginsCreatedBetweenImpl,
               PasswordStoreChangeList(base::Time, base::Time));
  MOCK_METHOD2(RemoveLoginsSyncedBetweenImpl,
               PasswordStoreChangeList(base::Time, base::Time));
  MOCK_METHOD3(RemoveStatisticsByOriginAndTimeImpl,
               bool(const base::Callback<bool(const GURL&)>&,
                    base::Time,
                    base::Time));
  MOCK_METHOD1(
      DisableAutoSignInForOriginsImpl,
      PasswordStoreChangeList(const base::Callback<bool(const GURL&)>&));
  std::vector<std::unique_ptr<autofill::PasswordForm>> FillMatchingLogins(
      const PasswordStore::FormDigest& form) override {
    return std::vector<std::unique_ptr<autofill::PasswordForm>>();
  }
  std::vector<std::unique_ptr<autofill::PasswordForm>>
  FillLoginsForSameOrganizationName(const std::string& signon_realm) override {
    return std::vector<std::unique_ptr<autofill::PasswordForm>>();
  }
  MOCK_METHOD1(FillAutofillableLogins,
               bool(std::vector<std::unique_ptr<autofill::PasswordForm>>*));
  MOCK_METHOD1(FillBlacklistLogins,
               bool(std::vector<std::unique_ptr<autofill::PasswordForm>>*));
  MOCK_METHOD1(NotifyLoginsChanged, void(const PasswordStoreChangeList&));
  MOCK_METHOD0(GetAllSiteStatsImpl, std::vector<InteractionsStats>());
  MOCK_METHOD1(GetSiteStatsImpl,
               std::vector<InteractionsStats>(const GURL& origin_domain));
  MOCK_METHOD1(AddSiteStatsImpl, void(const InteractionsStats&));
  MOCK_METHOD1(RemoveSiteStatsImpl, void(const GURL&));
// TODO(crbug.com/706392): Fix password reuse detection for Android.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  MOCK_METHOD3(CheckReuse,
               void(const base::string16&,
                    const std::string&,
                    PasswordReuseDetectorConsumer*));
#if !defined(OS_CHROMEOS)
  MOCK_METHOD1(SaveSyncPasswordHash, void(const base::string16&));
  MOCK_METHOD0(ClearSyncPasswordHash, void());
#endif
#endif

  PasswordStoreSync* GetSyncInterface() { return this; }

 protected:
  virtual ~MockPasswordStore();
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_
