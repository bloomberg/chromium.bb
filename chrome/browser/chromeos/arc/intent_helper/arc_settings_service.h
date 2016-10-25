// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_SETTINGS_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/instance_holder.h"

namespace arc {

class ArcBridgeService;
class ArcSettingsServiceImpl;

class ArcSettingsService
    : public ArcService,
      public InstanceHolder<mojom::IntentHelperInstance>::Observer {
 public:
  explicit ArcSettingsService(ArcBridgeService* bridge_service);
  ~ArcSettingsService() override;

  // InstanceHolder<mojom::IntentHelperInstance>::Observer
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

 private:
  std::unique_ptr<ArcSettingsServiceImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(ArcSettingsService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_SETTINGS_SERVICE_H_
