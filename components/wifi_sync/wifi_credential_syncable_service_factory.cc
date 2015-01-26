// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wifi_sync/wifi_credential_syncable_service_factory.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/wifi_sync/wifi_credential_syncable_service.h"

#if defined(OS_CHROMEOS)
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_handler.h"
#include "components/wifi_sync/wifi_config_delegate_chromeos.h"
#endif

namespace wifi_sync {

namespace {

scoped_ptr<WifiConfigDelegate> BuildConfigDelegate(
    content::BrowserContext* context) {
#if defined(OS_CHROMEOS)
  const chromeos::LoginState* login_state = chromeos::LoginState::Get();
  DCHECK(login_state->IsUserLoggedIn());
  DCHECK(!login_state->primary_user_hash().empty());
  // TODO(quiche): Verify that |context| is the primary user's context.

  // Note: NetworkHandler is a singleton that is managed by
  // ChromeBrowserMainPartsChromeos, and destroyed after all
  // KeyedService instances are destroyed.
  chromeos::NetworkHandler* network_handler = chromeos::NetworkHandler::Get();
  return make_scoped_ptr(new WifiConfigDelegateChromeOs(
      login_state->primary_user_hash(),
      network_handler->managed_network_configuration_handler()));
#else
  NOTREACHED();
  return nullptr;
#endif
}

}  // namespace

// static
WifiCredentialSyncableService*
WifiCredentialSyncableServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<WifiCredentialSyncableService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
WifiCredentialSyncableServiceFactory*
WifiCredentialSyncableServiceFactory::GetInstance() {
  return Singleton<WifiCredentialSyncableServiceFactory>::get();
}

// Private methods.

WifiCredentialSyncableServiceFactory::WifiCredentialSyncableServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "WifiCredentialSyncableService",
        BrowserContextDependencyManager::GetInstance()) {
}

WifiCredentialSyncableServiceFactory::~WifiCredentialSyncableServiceFactory() {
}

KeyedService* WifiCredentialSyncableServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // TODO(quiche): Figure out if this behaves properly for multi-profile.
  // crbug.com/430681.
  return new WifiCredentialSyncableService(BuildConfigDelegate(context));
}

}  // namespace wifi_sync
