// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYNC_FILE_SYSTEM_EXTENSION_SYNC_EVENT_OBSERVER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYNC_FILE_SYSTEM_EXTENSION_SYNC_EVENT_OBSERVER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class Profile;

namespace extensions {

class ExtensionSyncEventObserver;

class ExtensionSyncEventObserverFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionSyncEventObserver* GetForProfile(Profile* profile);

  static ExtensionSyncEventObserverFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionSyncEventObserverFactory>;

  ExtensionSyncEventObserverFactory();
  virtual ~ExtensionSyncEventObserverFactory();

  // BrowserContextKeyedServiceFactory implementation.
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYNC_FILE_SYSTEM_EXTENSION_SYNC_EVENT_OBSERVER_FACTORY_H_
