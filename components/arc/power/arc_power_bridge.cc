// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/power/arc_power_bridge.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "components/arc/arc_service_manager.h"

namespace arc {

ArcPowerBridge::ArcPowerBridge(ArcBridgeService* arc_bridge_service)
    : arc_bridge_service_(arc_bridge_service), binding_(this) {
  arc_bridge_service->AddObserver(this);
  if (arc_bridge_service->power_instance())
    OnPowerInstanceReady();
}

ArcPowerBridge::~ArcPowerBridge() {
  arc_bridge_service_->RemoveObserver(this);
  ReleaseAllDisplayWakeLocks();
}

void ArcPowerBridge::OnStateChanged(ArcBridgeService::State state) {
  if (state == ArcBridgeService::State::STOPPING)
    ReleaseAllDisplayWakeLocks();
}

void ArcPowerBridge::OnPowerInstanceReady() {
  PowerInstance* power_instance = arc_bridge_service_->power_instance();
  if (!power_instance) {
    LOG(ERROR) << "OnPowerInstanceReady called, but no power instance found";
    return;
  }

  PowerHostPtr host;
  binding_.Bind(mojo::GetProxy(&host));
  power_instance->Init(std::move(host));
}

void ArcPowerBridge::OnAcquireDisplayWakeLock(
    DisplayWakeLockType type) {
  if (!chromeos::PowerPolicyController::IsInitialized()) {
    LOG(WARNING) << "PowerPolicyController is not available";
    return;
  }
  chromeos::PowerPolicyController* controller =
      chromeos::PowerPolicyController::Get();

  int wake_lock_id = -1;
  switch (type) {
    case DISPLAY_WAKE_LOCK_TYPE_BRIGHT:
      wake_lock_id = controller->AddScreenWakeLock(
          chromeos::PowerPolicyController::REASON_OTHER, "ARC");
      break;
    case DISPLAY_WAKE_LOCK_TYPE_DIM:
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

void ArcPowerBridge::OnReleaseDisplayWakeLock(
    DisplayWakeLockType type) {
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

}  // namespace arc
