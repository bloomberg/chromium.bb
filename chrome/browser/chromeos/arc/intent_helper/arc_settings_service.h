// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_SETTINGS_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;
class ArcSettingsServiceImpl;

class ArcSettingsService
    : public KeyedService,
      public ConnectionObserver<mojom::IntentHelperInstance> {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcSettingsService* GetForBrowserContext(
      content::BrowserContext* context);

  ArcSettingsService(content::BrowserContext* context,
                     ArcBridgeService* bridge_service);
  ~ArcSettingsService() override;

  // ConnectionObserver<mojom::IntentHelperInstance>
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

 private:
  content::BrowserContext* const context_;
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  std::unique_ptr<ArcSettingsServiceImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(ArcSettingsService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_SETTINGS_SERVICE_H_
