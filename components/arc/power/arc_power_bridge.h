// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_POWER_ARC_POWER_BRIDGE_H_
#define COMPONENTS_ARC_POWER_ARC_POWER_BRIDGE_H_

#include <map>

#include "base/macros.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/arc/common/power.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/display/manager/chromeos/display_configurator.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// ARC Power Client sets power management policy based on requests from
// ARC instances.
class ArcPowerBridge : public KeyedService,
                       public InstanceHolder<mojom::PowerInstance>::Observer,
                       public chromeos::PowerManagerClient::Observer,
                       public display::DisplayConfigurator::Observer,
                       public mojom::PowerHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcPowerBridge* GetForBrowserContext(content::BrowserContext* context);

  ArcPowerBridge(content::BrowserContext* context,
                 ArcBridgeService* bridge_service);
  ~ArcPowerBridge() override;

  // InstanceHolder<mojom::PowerInstance>::Observer overrides.
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

  // chromeos::PowerManagerClient::Observer overrides.
  void SuspendImminent() override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;
  void BrightnessChanged(int level, bool user_initiated) override;

  // DisplayConfigurator::Observer overrides.
  void OnPowerStateChanged(chromeos::DisplayPowerState power_state) override;

  // mojom::PowerHost overrides.
  void OnAcquireDisplayWakeLock(mojom::DisplayWakeLockType type) override;
  void OnReleaseDisplayWakeLock(mojom::DisplayWakeLockType type) override;
  void IsDisplayOn(IsDisplayOnCallback callback) override;
  void OnScreenBrightnessUpdateRequest(double percent) override;

 private:
  void ReleaseAllDisplayWakeLocks();
  void UpdateAndroidScreenBrightness(double percent);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  mojo::Binding<mojom::PowerHost> binding_;

  // Stores a mapping of type -> wake lock ID for all wake locks
  // held by ARC.
  std::multimap<mojom::DisplayWakeLockType, int> wake_locks_;

  // Last time that the power manager notified about a brightness change.
  base::TimeTicks last_brightness_changed_time_;
  // Timer used to run UpdateAndroidScreenBrightness() to notify Android
  // about brightness changes.
  base::OneShotTimer notify_brightness_timer_;

  base::WeakPtrFactory<ArcPowerBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcPowerBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_POWER_ARC_POWER_BRIDGE_H_
