// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYNC_FILE_SYSTEM_EXTENSION_SYNC_EVENT_OBSERVER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYNC_FILE_SYSTEM_EXTENSION_SYNC_EVENT_OBSERVER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class ExtensionSyncEventObserver;

class ExtensionSyncEventObserverFactory : public ProfileKeyedServiceFactory {
 public:
  static ExtensionSyncEventObserver* GetForProfile(Profile* profile);

  static ExtensionSyncEventObserverFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionSyncEventObserverFactory>;

  ExtensionSyncEventObserverFactory();
  virtual ~ExtensionSyncEventObserverFactory();

  // ProfileKeyedServiceFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYNC_FILE_SYSTEM_EXTENSION_SYNC_EVENT_OBSERVER_FACTORY_H_
