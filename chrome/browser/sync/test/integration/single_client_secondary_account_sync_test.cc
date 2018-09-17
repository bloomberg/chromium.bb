// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/fake_gaia_cookie_manager_service_builder.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "services/identity/public/cpp/identity_test_utils.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#endif  // defined(OS_CHROMEOS)

namespace {

syncer::ModelTypeSet AllowedTypesInStandaloneTransportMode() {
  // Only some special whitelisted types (and control types) are allowed in
  // standalone transport mode.
  syncer::ModelTypeSet allowed_types(syncer::USER_CONSENTS,
                                     syncer::AUTOFILL_WALLET_DATA);
  allowed_types.PutAll(syncer::ControlTypes());
  return allowed_types;
}

class SingleClientSecondaryAccountSyncTest : public SyncTest {
 public:
  SingleClientSecondaryAccountSyncTest() : SyncTest(SINGLE_CLIENT) {
    features_.InitWithFeatures(
        /*enabled_features=*/{switches::kSyncStandaloneTransport,
                              switches::kSyncSupportSecondaryAccount},
        /*disabled_features=*/{});
  }
  ~SingleClientSecondaryAccountSyncTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::BindRepeating(&SingleClientSecondaryAccountSyncTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
#if defined(OS_CHROMEOS)
    InitNetwork();
#endif  // defined(OS_CHROMEOS)
  }

  Profile* profile() { return GetProfile(0); }

  // Makes a non-primary account available with both a refresh token and cookie.
  void SignInSecondaryAccount(const std::string& email) {
    identity::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    AccountInfo account_info = identity::MakeAccountAvailable(
        AccountTrackerServiceFactory::GetForProfile(profile()),
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile()),
        identity_manager, email);
    FakeGaiaCookieManagerService* fake_cookie_service =
        static_cast<FakeGaiaCookieManagerService*>(
            GaiaCookieManagerServiceFactory::GetForProfile(profile()));
    identity::SetCookieAccounts(fake_cookie_service, identity_manager,
                                {{account_info.email, account_info.gaia}});
  }

#if !defined(OS_CHROMEOS)
  // Makes the given account Chrome's primary one. The account must already be
  // signed in (per SignInSecondaryAccount).
  void MakeAccountPrimary(const std::string& email) {
    // This is implemented in the same way as in
    // DiceTurnSyncOnHelper::SigninAndShowSyncConfirmationUI.
    // TODO(blundell): IdentityManager should support this use case, so we don't
    // have to go through the legacy API.
    SigninManagerFactory::GetForProfile(profile())->OnExternalSigninCompleted(
        email);
  }
#endif  // !defined(OS_CHROMEOS)

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    GaiaCookieManagerServiceFactory::GetInstance()->SetTestingFactory(
        context, &BuildFakeGaiaCookieManagerService);
  }

#if defined(OS_CHROMEOS)
  // TODO(crbug.com/882770): On ChromeOS, we need to set up a fake
  // NetworkPortalDetector, otherwise chromeos::DelayNetworkCall will think it's
  // behind a captive portal and delay all network requests forever, which means
  // the ListAccounts requests (i.e. getting cookie accounts) will never make it
  // far enough to even request our fake response.
  void InitNetwork() {
    auto* portal_detector = new chromeos::NetworkPortalDetectorTestImpl();

    const chromeos::NetworkState* default_network =
        chromeos::NetworkHandler::Get()
            ->network_state_handler()
            ->DefaultNetwork();

    portal_detector->SetDefaultNetworkForTesting(default_network->guid());

    chromeos::NetworkPortalDetector::CaptivePortalState online_state;
    online_state.status =
        chromeos::NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
    online_state.response_code = 204;
    portal_detector->SetDetectionResultsForTesting(default_network->guid(),
                                                   online_state);

    // Takes ownership.
    chromeos::network_portal_detector::InitializeForTesting(portal_detector);
  }
#endif  // defined(OS_CHROMEOS)

  base::test::ScopedFeatureList features_;

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientSecondaryAccountSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       DoesNotStartSyncWithStandaloneTransportDisabled) {
  base::test::ScopedFeatureList disable_standalone_transport;
  disable_standalone_transport.InitAndDisableFeature(
      switches::kSyncStandaloneTransport);

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Since standalone transport is disabled, just signing in (without making the
  // account Chrome's primary one) should *not* start the Sync machinery.
  SignInSecondaryAccount("user@email.com");
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());
}

IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       DoesNotStartSyncWithSecondaryAccountSupportDisabled) {
  base::test::ScopedFeatureList disable_secondary_account_support;
  disable_secondary_account_support.InitAndDisableFeature(
      switches::kSyncSupportSecondaryAccount);

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Since secondary account support is disabled, just signing in (without
  // making the account Chrome's primary one) should *not* start the Sync
  // machinery.
  SignInSecondaryAccount("user@email.com");
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());
}

IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       StartsSyncTransportOnSignin) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Signing in (without making the account Chrome's primary one or explicitly
  // setting up Sync) should trigger starting the Sync machinery in standalone
  // transport mode.
  SignInSecondaryAccount("user@email.com");
  if (browser_defaults::kSyncAutoStarts) {
    EXPECT_EQ(syncer::SyncService::TransportState::INITIALIZING,
              GetSyncService(0)->GetTransportState());
  } else {
    EXPECT_EQ(syncer::SyncService::TransportState::START_DEFERRED,
              GetSyncService(0)->GetTransportState());
  }

  EXPECT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));

  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  ASSERT_EQ(browser_defaults::kSyncAutoStarts,
            GetSyncService(0)->IsFirstSetupComplete());

  EXPECT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(GetSyncService(0)->IsSyncFeatureActive());

  // Make sure that only the allowed types got activated. Note that, depending
  // on some other feature flags, not all of the allowed types are necessarily
  // active, and that's okay.
  syncer::ModelTypeSet bad_types =
      syncer::Difference(GetSyncService(0)->GetActiveDataTypes(),
                         AllowedTypesInStandaloneTransportMode());
  EXPECT_TRUE(bad_types.Empty()) << syncer::ModelTypeSetToString(bad_types);
}

// ChromeOS doesn't support changes to the primary account after startup, so
// this test doesn't apply.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       SwitchesFromTransportToFeature) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Set up Sync in transport mode for a non-primary account.
  SignInSecondaryAccount("user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());

  // Simulate the user opting in to full Sync: Make the account primary, and
  // set first-time setup to complete.
  MakeAccountPrimary("user@email.com");
  GetSyncService(0)->SetFirstSetupComplete();

  EXPECT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  EXPECT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // Make sure that some model type which is not allowed in transport-only mode
  // got activated.
  ASSERT_FALSE(AllowedTypesInStandaloneTransportMode().Has(syncer::BOOKMARKS));
  ASSERT_TRUE(
      GetSyncService(0)->GetPreferredDataTypes().Has(syncer::BOOKMARKS));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::BOOKMARKS));
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace
