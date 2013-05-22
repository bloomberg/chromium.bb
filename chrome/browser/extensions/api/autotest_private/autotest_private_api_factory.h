// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_FACTORY_H__
#define CHROME_BROWSER_EXTENSIONS_API_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_FACTORY_H__

#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class Profile;

namespace extensions {
class AutotestPrivateAPI;

class AutotestPrivateAPIFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AutotestPrivateAPI* GetForProfile(Profile* profile);

  static AutotestPrivateAPIFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<AutotestPrivateAPIFactory>;

  AutotestPrivateAPIFactory();
  virtual ~AutotestPrivateAPIFactory();

  // BrowserContextKeyedBaseFactory implementation.
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_FACTORY_H__
