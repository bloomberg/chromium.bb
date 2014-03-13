// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_MANAGER_FACTORY_H__
#define CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_MANAGER_FACTORY_H__

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace extensions {
class IdleManager;

class IdleManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static IdleManager* GetForProfile(Profile* profile);

  static IdleManagerFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<IdleManagerFactory>;

  IdleManagerFactory();
  virtual ~IdleManagerFactory();

  // BrowserContextKeyedBaseFactory implementation.
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_MANAGER_FACTORY_H__
