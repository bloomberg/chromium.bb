// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_driver_switches.h"

namespace {

syncer::ModelTypeSet AllowedTypesInStandaloneTransportMode() {
  // Only some special whitelisted types (and control types) are allowed in
  // standalone transport mode.
  syncer::ModelTypeSet allowed_types(syncer::USER_CONSENTS,
                                     syncer::AUTOFILL_WALLET_DATA);
  allowed_types.PutAll(syncer::ControlTypes());
  return allowed_types;
}

class SingleClientStandaloneTransportSyncTest : public SyncTest {
 public:
  SingleClientStandaloneTransportSyncTest() : SyncTest(SINGLE_CLIENT) {
    features_.InitAndEnableFeature(switches::kSyncStandaloneTransport);
  }
  ~SingleClientStandaloneTransportSyncTest() override {}

 private:
  base::test::ScopedFeatureList features_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientStandaloneTransportSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       DoesNotStartSyncWithFeatureDisabled) {
  base::test::ScopedFeatureList disable_standalone_transport;
  disable_standalone_transport.InitAndDisableFeature(
      switches::kSyncStandaloneTransport);

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Since standalone transport is disabled, signing in should *not* start the
  // Sync machinery.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  EXPECT_EQ(syncer::SyncService::TransportState::WAITING_FOR_START_REQUEST,
            GetSyncService(0)->GetTransportState());
}

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       StartsSyncFeatureOnSignin) {
  // On platforms where Sync starts automatically (in practice, Android and
  // ChromeOS), IsFirstSetupComplete gets set automatically, and so the full
  // Sync feature will start upon sign-in to a primary account.
  ASSERT_TRUE(browser_defaults::kSyncAutoStarts);

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Signing in (without explicitly setting up Sync) should trigger starting the
  // Sync machinery. Because IsFirstSetupComplete gets set automatically, this
  // will actually start the full Sync feature, not just the transport.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  EXPECT_EQ(syncer::SyncService::TransportState::INITIALIZING,
            GetSyncService(0)->GetTransportState());

  EXPECT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));

  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  ASSERT_TRUE(GetSyncService(0)->IsFirstSetupComplete());

  EXPECT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(GetSyncService(0)->IsSyncActive());
}
#else
IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       StartsSyncTransportOnSignin) {
  ASSERT_FALSE(browser_defaults::kSyncAutoStarts);

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Signing in (without explicitly setting up Sync) should trigger starting the
  // Sync machinery in standalone transport mode.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  EXPECT_EQ(syncer::SyncService::TransportState::START_DEFERRED,
            GetSyncService(0)->GetTransportState());

  EXPECT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));

  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  ASSERT_FALSE(GetSyncService(0)->IsFirstSetupComplete());

  EXPECT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(GetSyncService(0)->IsSyncActive());

  // Make sure that only the allowed types got activated. Note that, depending
  // on some other feature flags, not all of the allowed types are necessarily
  // active, and that's okay.
  syncer::ModelTypeSet bad_types =
      syncer::Difference(GetSyncService(0)->GetActiveDataTypes(),
                         AllowedTypesInStandaloneTransportMode());
  EXPECT_TRUE(bad_types.Empty()) << syncer::ModelTypeSetToString(bad_types);
}
#endif  // defined(OS_CHROMEOS) || defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       SwitchesBetweenTransportAndFeature) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Set up Sync-the-feature.
  ASSERT_TRUE(GetClient(0)->SetupSync());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncActive());

  // Make sure that some model type which is not allowed in transport-only mode
  // got activated.
  ASSERT_FALSE(AllowedTypesInStandaloneTransportMode().Has(syncer::BOOKMARKS));
  ASSERT_TRUE(
      GetSyncService(0)->GetPreferredDataTypes().Has(syncer::BOOKMARKS));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::BOOKMARKS));

  // Turn off Sync-the-feature by user choice. The machinery should start up
  // again in transport-only mode.
  GetSyncService(0)->RequestStop(syncer::SyncService::KEEP_DATA);
  EXPECT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));

  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  EXPECT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(GetSyncService(0)->IsSyncActive());

  syncer::ModelTypeSet bad_types =
      syncer::Difference(GetSyncService(0)->GetActiveDataTypes(),
                         AllowedTypesInStandaloneTransportMode());
  EXPECT_TRUE(bad_types.Empty()) << syncer::ModelTypeSetToString(bad_types);

  // Finally, turn Sync-the-feature on again.
  GetSyncService(0)->RequestStart();
  EXPECT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  EXPECT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(GetSyncService(0)->IsSyncActive());
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::BOOKMARKS));
}

}  // namespace
