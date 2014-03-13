// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class ExtensionSyncService;
class Profile;

class ExtensionSyncServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionSyncService* GetForProfile(Profile* profile);

  static ExtensionSyncServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionSyncServiceFactory>;

  ExtensionSyncServiceFactory();
  virtual ~ExtensionSyncServiceFactory();

  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_FACTORY_H_
