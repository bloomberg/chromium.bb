// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_ENTRY_WATCHER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_ENTRY_WATCHER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class EntryWatcherService;

// Creates instances of the EntryWatcherService class.
class EntryWatcherServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns a service instance singleton per |context|, after creating it
  // (if necessary).
  static EntryWatcherService* Get(content::BrowserContext* context);

  // Returns a service instance for the context if exists. Otherwise, returns
  // NULL.
  static EntryWatcherService* FindExisting(content::BrowserContext* context);

  // Gets a singleton instance of the factory.
  static EntryWatcherServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<EntryWatcherServiceFactory>;

  EntryWatcherServiceFactory();
  virtual ~EntryWatcherServiceFactory();

  // BrowserContextKeyedServiceFactory overrides:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(EntryWatcherServiceFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_ENTRY_WATCHER_SERVICE_FACTORY_H_
