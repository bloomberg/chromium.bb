// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/power/arc_power_bridge.h"

#include <algorithm>
#include <utility>

#include "ash/shell.h"
#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"

namespace arc {

// Delay for notifying Android about screen brightness changes, added in
// order to prevent spammy brightness updates.
constexpr base::TimeDelta kNotifyBrightnessDelay =
    base::TimeDelta::FromMilliseconds(200);

ArcPowerBridge::ArcPowerBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this), weak_ptr_factory_(this) {
  arc_bridge_service()->power()->AddObserver(this);
}

ArcPowerBridge::~ArcPowerBridge() {
  arc_bridge_service()->power()->RemoveObserver(this);
  ReleaseAllDisplayWakeLocks();
}

void ArcPowerBridge::OnInstanceReady() {
  mojom::PowerInstance* power_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service()->power(), Init);
  DCHECK(power_instance);
  power_instance->Init(binding_.CreateInterfacePtrAndBind());
  ash::Shell::Get()->display_configurator()->AddObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      AddObserver(this);
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->GetScreenBrightnessPercent(
          base::Bind(&ArcPowerBridge::UpdateAndroidScreenBrightness,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcPowerBridge::OnInstanceClosed() {
  ash::Shell::Get()->display_configurator()->RemoveObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RemoveObserver(this);
  ReleaseAllDisplayWakeLocks();
}

void ArcPowerBridge::SuspendImminent() {
  mojom::PowerInstance* power_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->power(), Suspend);
  if (!power_instance)
    return;

  power_instance->Suspend(
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
        GetSuspendReadinessCallback());
}

void ArcPowerBridge::SuspendDone(const base::TimeDelta& sleep_duration) {
  mojom::PowerInstance* power_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->power(), Resume);
  if (!power_instance)
    return;

  power_instance->Resume();
}

void ArcPowerBridge::BrightnessChanged(int level, bool user_initiated) {
  double percent = static_cast<double>(level);
  const base::TimeTicks now = base::TimeTicks::Now();
  if (last_brightness_changed_time_.is_null() ||
      (now - last_brightness_changed_time_) >= kNotifyBrightnessDelay) {
    UpdateAndroidScreenBrightness(percent);
    notify_brightness_timer_.Stop();
  } else {
    notify_brightness_timer_.Start(
        FROM_HERE, kNotifyBrightnessDelay,
        base::Bind(&ArcPowerBridge::UpdateAndroidScreenBrightness,
                   weak_ptr_factory_.GetWeakPtr(), percent));
  }
  last_brightness_changed_time_ = now;
}

void ArcPowerBridge::OnPowerStateChanged(
    chromeos::DisplayPowerState power_state) {
  mojom::PowerInstance* power_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->power(), SetInteractive);
  if (!power_instance)
    return;

  bool enabled = (power_state != chromeos::DISPLAY_POWER_ALL_OFF);
  power_instance->SetInteractive(enabled);
}

void ArcPowerBridge::OnAcquireDisplayWakeLock(mojom::DisplayWakeLockType type) {
  if (!chromeos::PowerPolicyController::IsInitialized()) {
    LOG(WARNING) << "PowerPolicyController is not available";
    return;
  }
  chromeos::PowerPolicyController* controller =
      chromeos::PowerPolicyController::Get();

  int wake_lock_id = -1;
  switch (type) {
    case mojom::DisplayWakeLockType::BRIGHT:
      wake_lock_id = controller->AddScreenWakeLock(
          chromeos::PowerPolicyController::REASON_OTHER, "ARC");
      break;
    case mojom::DisplayWakeLockType::DIM:
      wake_lock_id = controller->AddDimWakeLock(
          chromeos::PowerPolicyController::REASON_OTHER, "ARC");
      break;
    default:
      LOG(WARNING) << "Tried to take invalid wake lock type "
                   << static_cast<int>(type);
      return;
  }
  wake_locks_.insert(std::make_pair(type, wake_lock_id));
}

void ArcPowerBridge::OnReleaseDisplayWakeLock(mojom::DisplayWakeLockType type) {
  if (!chromeos::PowerPolicyController::IsInitialized()) {
    LOG(WARNING) << "PowerPolicyController is not available";
    return;
  }
  chromeos::PowerPolicyController* controller =
      chromeos::PowerPolicyController::Get();

  // From the perspective of the PowerPolicyController, all wake locks
  // of a given type are equivalent, so it doesn't matter which one
  // we pass to the controller here.
  auto it = wake_locks_.find(type);
  if (it == wake_locks_.end()) {
    LOG(WARNING) << "Tried to release wake lock of type "
                 << static_cast<int>(type) << " when none were taken";
    return;
  }
  controller->RemoveWakeLock(it->second);
  wake_locks_.erase(it);
}

void ArcPowerBridge::IsDisplayOn(const IsDisplayOnCallback& callback) {
  callback.Run(ash::Shell::Get()->display_configurator()->IsDisplayOn());
}

void ArcPowerBridge::OnScreenBrightnessUpdateRequest(double percent) {
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->SetScreenBrightnessPercent(percent, true);
}

void ArcPowerBridge::ReleaseAllDisplayWakeLocks() {
  if (!chromeos::PowerPolicyController::IsInitialized()) {
    LOG(WARNING) << "PowerPolicyController is not available";
    return;
  }
  chromeos::PowerPolicyController* controller =
      chromeos::PowerPolicyController::Get();

  for (const auto& it : wake_locks_) {
    controller->RemoveWakeLock(it.second);
  }
  wake_locks_.clear();
}

void ArcPowerBridge::UpdateAndroidScreenBrightness(double percent) {
  mojom::PowerInstance* power_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->power(), UpdateScreenBrightnessSettings);
  if (!power_instance)
    return;
  power_instance->UpdateScreenBrightnessSettings(percent);
}

}  // namespace arc
