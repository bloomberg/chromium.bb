// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/secondary_account_helper.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/fake_gaia_cookie_manager_service_builder.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "services/identity/public/cpp/primary_account_mutator.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#endif  // defined(OS_CHROMEOS)

namespace secondary_account_helper {

namespace {

void OnWillCreateBrowserContextServices(
    network::TestURLLoaderFactory* test_url_loader_factory,
    content::BrowserContext* context) {
  GaiaCookieManagerServiceFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(&BuildFakeGaiaCookieManagerServiceWithURLLoader,
                          test_url_loader_factory));
}

}  // namespace

ScopedFakeGaiaCookieManagerServiceFactory SetUpFakeGaiaCookieManagerService(
    network::TestURLLoaderFactory* test_url_loader_factory) {
  return BrowserContextDependencyManager::GetInstance()
      ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
          base::BindRepeating(&OnWillCreateBrowserContextServices,
                              test_url_loader_factory));
}

#if defined(OS_CHROMEOS)
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

void SignInSecondaryAccount(Profile* profile, const std::string& email) {
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountInfo account_info =
      identity::MakeAccountAvailable(identity_manager, email);
  FakeGaiaCookieManagerService* fake_cookie_service =
      static_cast<FakeGaiaCookieManagerService*>(
          GaiaCookieManagerServiceFactory::GetForProfile(profile));
  identity::SetCookieAccounts(fake_cookie_service, identity_manager,
                              {{account_info.email, account_info.gaia}});
}

#if !defined(OS_CHROMEOS)
void MakeAccountPrimary(Profile* profile, const std::string& email) {
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  base::Optional<AccountInfo> maybe_account =
      identity_manager->FindAccountInfoForAccountWithRefreshTokenByEmailAddress(
          email);
  DCHECK(maybe_account.has_value());
  auto* primary_account_mutator = identity_manager->GetPrimaryAccountMutator();
  primary_account_mutator->SetPrimaryAccount(maybe_account->account_id);
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace secondary_account_helper
