// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_BOOT_PHASE_MONITOR_ARC_BOOT_PHASE_MONITOR_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_BOOT_PHASE_MONITOR_ARC_BOOT_PHASE_MONITOR_BRIDGE_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/boot_phase_monitor.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/signin/core/account_id/account_id.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class ArcBridgeService;
class ArcInstanceThrottle;

// Receives boot phase notifications from ARC.
class ArcBootPhaseMonitorBridge
    : public ArcService,
      public InstanceHolder<mojom::BootPhaseMonitorInstance>::Observer,
      public mojom::BootPhaseMonitorHost {
 public:
  ArcBootPhaseMonitorBridge(ArcBridgeService* bridge_service,
                            const AccountId& account_id);
  ~ArcBootPhaseMonitorBridge() override;

  // InstanceHolder<mojom::BootPhaseMonitorInstance>::Observer
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

  // mojom::BootPhaseMonitorHost
  void OnBootCompleted() override;

 private:
  const AccountId account_id_;
  mojo::Binding<mojom::BootPhaseMonitorHost> binding_;
  std::unique_ptr<ArcInstanceThrottle> throttle_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ArcBootPhaseMonitorBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_BOOT_PHASE_MONITOR_ARC_BOOT_PHASE_MONITOR_BRIDGE_H_
