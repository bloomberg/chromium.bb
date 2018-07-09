// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/macros.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/protocol/user_consent_specifics.pb.h"

using consent_auditor::ConsentStatus;
using consent_auditor::Feature;
using fake_server::FakeServer;
using sync_pb::SyncEntity;
using sync_pb::UserConsentSpecifics;

namespace {

std::string GetAccountId() {
#if defined(OS_CHROMEOS)
  // TODO(vitaliii): Unify the two, because it takes ages to debug and
  // impossible to discover otherwise.
  return "user@gmail.com";
#else
  return "gaia-id-user@gmail.com";
#endif
}

class UserConsentEqualityChecker : public SingleClientStatusChangeChecker {
 public:
  UserConsentEqualityChecker(
      browser_sync::ProfileSyncService* service,
      FakeServer* fake_server,
      std::vector<UserConsentSpecifics> expected_specifics)
      : SingleClientStatusChangeChecker(service), fake_server_(fake_server) {
    for (const UserConsentSpecifics& specifics : expected_specifics) {
      expected_specifics_.insert(std::pair<int64_t, UserConsentSpecifics>(
          specifics.confirmation_grd_id(), specifics));
    }
  }

  bool IsExitConditionSatisfied() override {
    std::vector<SyncEntity> entities =
        fake_server_->GetSyncEntitiesByModelType(syncer::USER_CONSENTS);

    // |entities.size()| is only going to grow, if |entities.size()| ever
    // becomes bigger then all hope is lost of passing, stop now.
    EXPECT_GE(expected_specifics_.size(), entities.size());

    if (expected_specifics_.size() > entities.size()) {
      return false;
    }

    // Number of events on server matches expected, exit condition can be
    // satisfied. Let's verify that content matches as well. It is safe to
    // modify |expected_specifics_|.
    for (const SyncEntity& entity : entities) {
      UserConsentSpecifics server_specifics = entity.specifics().user_consent();
      auto iter =
          expected_specifics_.find(server_specifics.confirmation_grd_id());
      EXPECT_TRUE(expected_specifics_.end() != iter);
      if (expected_specifics_.end() == iter) {
        return false;
      };
      EXPECT_EQ(iter->second.account_id(), server_specifics.account_id());
      expected_specifics_.erase(iter);
    }

    return true;
  }

  std::string GetDebugMessage() const override {
    return "Waiting server side USER_CONSENTS to match expected.";
  }

 private:
  FakeServer* fake_server_;
  std::multimap<int64_t, UserConsentSpecifics> expected_specifics_;

  DISALLOW_COPY_AND_ASSIGN(UserConsentEqualityChecker);
};

class SingleClientUserConsentsSyncTest : public SyncTest {
 public:
  SingleClientUserConsentsSyncTest() : SyncTest(SINGLE_CLIENT) {
    DisableVerifier();
  }

  bool ExpectUserConsents(
      std::vector<UserConsentSpecifics> expected_specifics) {
    return UserConsentEqualityChecker(GetSyncService(0), GetFakeServer(),
                                      expected_specifics)
        .Wait();
  }

  void SetSyncUserConsentSeparateTypeFeature(bool value) {
    SyncTest::feature_list_.InitWithFeatureState(
        switches::kSyncUserConsentSeparateType, value);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientUserConsentsSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientUserConsentsSyncTest,
                       ShouldSubmitAsSeparateConsentDatatypeWhenEnabled) {
  SetSyncUserConsentSeparateTypeFeature(true);

  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(0u, GetFakeServer()
                    ->GetSyncEntitiesByModelType(syncer::USER_CONSENTS)
                    .size());
  consent_auditor::ConsentAuditor* consent_service =
      ConsentAuditorFactory::GetForProfile(GetProfile(0));
  UserConsentSpecifics specifics;
  specifics.set_confirmation_grd_id(1);
  specifics.set_account_id(GetAccountId());
  consent_service->RecordGaiaConsent(
      GetAccountId(), Feature::CHROME_SYNC, /*description_grd_ids=*/{},
      /*confirmation_grd_id=*/1, ConsentStatus::GIVEN);
  EXPECT_TRUE(ExpectUserConsents({specifics}));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientUserConsentsSyncTest,
    ShouldPreserveConsentsOnDisableSyncAndResubmitWhenReneabled) {
  SetSyncUserConsentSeparateTypeFeature(true);

  UserConsentSpecifics specifics;
  specifics.set_confirmation_grd_id(1);
  // Account id may be compared to the synced account, thus, we need them to
  // match.
  specifics.set_account_id(GetAccountId());

  ASSERT_TRUE(SetupSync());
  consent_auditor::ConsentAuditor* consent_service =
      ConsentAuditorFactory::GetForProfile(GetProfile(0));
  consent_service->RecordGaiaConsent(
      GetAccountId(), Feature::CHROME_SYNC, /*description_grd_ids=*/{},
      /*confirmation_grd_id=*/1, ConsentStatus::GIVEN);

  GetClient(0)->StopSyncService(syncer::SyncService::CLEAR_DATA);
  ASSERT_TRUE(GetClient(0)->StartSyncService());

  EXPECT_TRUE(ExpectUserConsents({specifics}));
}

}  // namespace
