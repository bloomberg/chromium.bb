// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_service_launcher.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/arc/arc_policy_bridge.h"
#include "chrome/browser/chromeos/arc/arc_process_service.h"
#include "chrome/browser/chromeos/arc/arc_settings_service.h"
#include "chrome/browser/chromeos/arc/gpu_arc_video_service_host.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"

namespace arc {

ArcServiceLauncher::ArcServiceLauncher() : weak_factory_(this) {}

ArcServiceLauncher::~ArcServiceLauncher() {}

void ArcServiceLauncher::Initialize() {
  // Create ARC service manager.
  arc_service_manager_.reset(
      new ArcServiceManager(content::BrowserThread::GetBlockingPool()));
  arc_service_manager_->AddService(base::WrapUnique(
      new ArcAuthService(arc_service_manager_->arc_bridge_service())));
  arc_service_manager_->AddService(base::WrapUnique(
      new ArcPolicyBridge(arc_service_manager_->arc_bridge_service())));
  arc_service_manager_->AddService(base::WrapUnique(
      new ArcProcessService(arc_service_manager_->arc_bridge_service())));
  arc_service_manager_->AddService(base::WrapUnique(
      new ArcSettingsService(arc_service_manager_->arc_bridge_service())));
  arc_service_manager_->AddService(base::WrapUnique(
      new GpuArcVideoServiceHost(arc_service_manager_->arc_bridge_service())));

  // Detect availability.
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->CheckArcAvailability(base::Bind(
      &ArcServiceLauncher::OnArcAvailable, weak_factory_.GetWeakPtr()));
}

void ArcServiceLauncher::Shutdown() {
  DCHECK(arc_service_manager_);
  arc_service_manager_->Shutdown();
  arc_service_manager_->arc_bridge_service()->Shutdown();
}

void ArcServiceLauncher::OnArcAvailable(bool arc_available) {
  arc_service_manager_->arc_bridge_service()->SetDetectedAvailability(
      arc_available);
}

}  // namespace arc
