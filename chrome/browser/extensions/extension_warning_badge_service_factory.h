// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WARNING_BADGE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WARNING_BADGE_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class ExtensionWarningBadgeService;

class ExtensionWarningBadgeServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionWarningBadgeService* GetForBrowserContext(
      content::BrowserContext* context);
  static ExtensionWarningBadgeServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionWarningBadgeServiceFactory>;

  ExtensionWarningBadgeServiceFactory();
  ~ExtensionWarningBadgeServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWarningBadgeServiceFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WARNING_BADGE_SERVICE_FACTORY_H_
