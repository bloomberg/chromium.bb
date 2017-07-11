// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace arc {

class ArcAuthService;

// Factory for ArcAuthService.
class ArcAuthServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the ArcAuthService for the given |context|,
  // or nullptr if |context| is not allowed to use ARC.
  // If the instance is not yet createad, this creates a new service instance.
  static ArcAuthService* GetForBrowserContext(content::BrowserContext* context);

  // Returns the factory instance.
  static ArcAuthServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ArcAuthServiceFactory>;

  ArcAuthServiceFactory();
  ~ArcAuthServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthServiceFactory);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_SERVICE_FACTORY_H_
