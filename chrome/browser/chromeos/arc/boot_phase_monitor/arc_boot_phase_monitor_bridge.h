// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_BOOT_PHASE_MONITOR_ARC_BOOT_PHASE_MONITOR_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_BOOT_PHASE_MONITOR_ARC_BOOT_PHASE_MONITOR_BRIDGE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "components/arc/common/boot_phase_monitor.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;
class ArcInstanceThrottle;

// Receives boot phase notifications from ARC.
// TODO(yusukes): Add unit tests for this.
class ArcBootPhaseMonitorBridge
    : public KeyedService,
      public InstanceHolder<mojom::BootPhaseMonitorInstance>::Observer,
      public mojom::BootPhaseMonitorHost,
      public ArcSessionManager::Observer {
 public:
  using FirstAppLaunchDelayRecorder =
      base::RepeatingCallback<void(base::TimeDelta)>;

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcBootPhaseMonitorBridge* GetForBrowserContext(
      content::BrowserContext* context);

  // Records Arc.FirstAppLaunchDelay.TimeDelta UMA in the following way:
  //
  // * If ARC has already fully started, record the UMA with 0.
  // * If ARC hasn't fully started yet, record the UMA in OnBootCompleted()
  //   later.
  // * If |first_app_launch_delay_recorded_| is true, do nothing.
  //
  // This function must be called every time when Chrome browser tries to launch
  // an ARC app.
  static void RecordFirstAppLaunchDelayUMA(content::BrowserContext* context);

  ArcBootPhaseMonitorBridge(content::BrowserContext* context,
                            ArcBridgeService* bridge_service);
  ~ArcBootPhaseMonitorBridge() override;

  // InstanceHolder<mojom::BootPhaseMonitorInstance>::Observer
  void OnInstanceReady() override;

  // mojom::BootPhaseMonitorHost
  void OnBootCompleted() override;

  // ArcSessionManager::Observer
  void OnArcInitialStart() override;
  void OnArcSessionStopped(ArcStopReason stop_reason) override;
  void OnArcSessionRestarting() override;

  void RecordFirstAppLaunchDelayUMAForTesting() {
    RecordFirstAppLaunchDelayUMAInternal();
  }

  ArcInstanceThrottle* throttle_for_testing() const { return throttle_.get(); }
  void set_first_app_launch_delay_recorder_for_testing(
      const FirstAppLaunchDelayRecorder& first_app_launch_delay_recorder) {
    first_app_launch_delay_recorder_ = first_app_launch_delay_recorder;
  }

 private:
  void RecordFirstAppLaunchDelayUMAInternal();
  void Reset();

  THREAD_CHECKER(thread_checker_);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  const AccountId account_id_;
  mojo::Binding<mojom::BootPhaseMonitorHost> binding_;
  FirstAppLaunchDelayRecorder first_app_launch_delay_recorder_;

  // The following variables must be reset every time when the instance stops or
  // restarts.
  std::unique_ptr<ArcInstanceThrottle> throttle_;
  base::TimeTicks app_launch_time_;
  bool first_app_launch_delay_recorded_ = false;
  bool boot_completed_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcBootPhaseMonitorBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_BOOT_PHASE_MONITOR_ARC_BOOT_PHASE_MONITOR_BRIDGE_H_
