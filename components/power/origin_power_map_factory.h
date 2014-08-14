// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_ORIGIN_POWER_MAP_FACTORY_H_
#define COMPONENTS_POWER_ORIGIN_POWER_MAP_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace power {

class OriginPowerMap;

class OriginPowerMapFactory : public BrowserContextKeyedServiceFactory {
 public:
  static OriginPowerMap* GetForBrowserContext(content::BrowserContext* context);
  static OriginPowerMapFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<OriginPowerMapFactory>;

  OriginPowerMapFactory();
  virtual ~OriginPowerMapFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(OriginPowerMapFactory);
};

}  // namespace power

#endif  // COMPONENTS_POWER_ORIGIN_POWER_MAP_FACTORY_H_
