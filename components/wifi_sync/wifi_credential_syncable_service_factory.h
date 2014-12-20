// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WIFI_SYNC_WIFI_CREDENTIAL_SYNCABLE_SERVICE_FACTORY_H_
#define COMPONENTS_WIFI_SYNC_WIFI_CREDENTIAL_SYNCABLE_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace wifi_sync {

class WifiCredentialSyncableService;

// Singleton that owns all WifiCredentialSyncableServices and
// associates them with Profiles. Listens for the Profile's
// destruction notification and cleans up the associated
// WifiCredentialSyncableServices.
class WifiCredentialSyncableServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the SyncableService for |browser_context|, creating the
  // SyncableService if one does not already exist.
  static WifiCredentialSyncableService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  // Returns the singleton instance. As this class has no public
  // instance methods, this function is not generally useful for
  // external callers. This function is public only so that the
  // Singleton template can reference it.
  static WifiCredentialSyncableServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<WifiCredentialSyncableServiceFactory>;

  WifiCredentialSyncableServiceFactory();
  ~WifiCredentialSyncableServiceFactory() override;

  // Implementation of BrowserContextKeyedServiceFactory.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(WifiCredentialSyncableServiceFactory);
};

}  // namespace wifi_sync

#endif  // COMPONENTS_WIFI_SYNC_WIFI_CREDENTIAL_SYNCABLE_SERVICE_FACTORY_H_
