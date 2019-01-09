// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/secondary_account_sync_test.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/signin_manager.h"
#include "services/identity/public/cpp/identity_test_utils.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
void SecondaryAccountSyncTest::InitNetwork() {
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

void SecondaryAccountSyncTest::SignInSecondaryAccount(
    Profile* profile,
    const std::string& email) {
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountInfo account_info =
      identity::MakeAccountAvailable(identity_manager, email);
  identity::SetCookieAccounts(identity_manager, &test_url_loader_factory_,
                              {{account_info.email, account_info.gaia}});
}

#if !defined(OS_CHROMEOS)
void SecondaryAccountSyncTest::MakeAccountPrimary(Profile* profile,
                                                  const std::string& email) {
  // This is implemented in the same way as in
  // DiceTurnSyncOnHelper::SigninAndShowSyncConfirmationUI.
  // TODO(blundell): IdentityManager should support this use case, so we don't
  // have to go through the legacy API.
  SigninManagerFactory::GetForProfile(profile)->OnExternalSigninCompleted(
      email);
}
#endif  // !defined(OS_CHROMEOS)
