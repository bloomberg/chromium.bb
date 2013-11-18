// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_RUNTIME_RUNTIME_API_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_RUNTIME_RUNTIME_API_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class Profile;

namespace extensions {

class RuntimeAPI;

// A RuntimeAPI object is created for every BrowserContext.
class RuntimeAPIFactory : public BrowserContextKeyedServiceFactory {
 public:
  static RuntimeAPI* GetForBrowserContext(content::BrowserContext* context);

  static RuntimeAPIFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<RuntimeAPIFactory>;

  RuntimeAPIFactory();
  virtual ~RuntimeAPIFactory();

  // BrowserContextKeyedServiceFactory implementation.
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(RuntimeAPIFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_RUNTIME_RUNTIME_API_FACTORY_H_
