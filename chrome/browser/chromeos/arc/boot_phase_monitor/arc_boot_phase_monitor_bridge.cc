// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_instance_throttle.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

ArcBootPhaseMonitorBridge::ArcBootPhaseMonitorBridge(
    ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->boot_phase_monitor()->AddObserver(this);
}

ArcBootPhaseMonitorBridge::~ArcBootPhaseMonitorBridge() {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->boot_phase_monitor()->RemoveObserver(this);
}

void ArcBootPhaseMonitorBridge::OnInstanceReady() {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->boot_phase_monitor(), Init);
  DCHECK(instance);
  instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcBootPhaseMonitorBridge::OnInstanceClosed() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ArcBootPhaseMonitorBridge::OnBootCompleted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(2) << "OnBootCompleted";

  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->EmitArcBooted();

  // Start monitoring window activation changes to prioritize/throttle the
  // container when needed.
  throttle_ = base::MakeUnique<ArcInstanceThrottle>();
}

}  // namespace arc
