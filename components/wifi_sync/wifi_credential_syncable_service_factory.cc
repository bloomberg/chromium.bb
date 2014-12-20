// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wifi_sync/wifi_credential_syncable_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/wifi_sync/wifi_credential_syncable_service.h"

namespace wifi_sync {

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
  return new WifiCredentialSyncableService();
}

}  // namespace wifi_sync
