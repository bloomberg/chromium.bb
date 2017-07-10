// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace arc {

class ArcAccessibilityHelperBridge;

// Factory for ArcAccessibilityHelperBridge.
class ArcAccessibilityHelperBridgeFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the ArcAccessibilityHelperBridge for the given |context|,
  // or nullptr if |context| is not allowed to use ARC.
  // If the instance is not yet created, this creates a new service instance.
  static ArcAccessibilityHelperBridge* GetForBrowserContext(
      content::BrowserContext* context);

  // Return the factory instance.
  static ArcAccessibilityHelperBridgeFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      ArcAccessibilityHelperBridgeFactory>;

  ArcAccessibilityHelperBridgeFactory();
  ~ArcAccessibilityHelperBridgeFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ArcAccessibilityHelperBridgeFactory);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_FACTORY_H_
