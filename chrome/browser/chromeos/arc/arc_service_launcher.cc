// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_service_launcher.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/arc/arc_intent_helper_bridge.h"
#include "chrome/browser/chromeos/arc/arc_process_service.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

ArcServiceLauncher::ArcServiceLauncher() : weak_factory_(this) {}

ArcServiceLauncher::~ArcServiceLauncher() {}

void ArcServiceLauncher::Initialize() {
  // Create ARC service manager.
  arc_service_manager_.reset(new ArcServiceManager());
  arc_service_manager_->AddService(make_scoped_ptr(
      new ArcAuthService(arc_service_manager_->arc_bridge_service())));
  arc_service_manager_->AddService(make_scoped_ptr(
      new ArcIntentHelperBridge(arc_service_manager_->arc_bridge_service())));
  arc_service_manager_->AddService(make_scoped_ptr(
      new ArcProcessService(arc_service_manager_->arc_bridge_service())));

  // Detect availiability.
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->CheckArcAvailability(base::Bind(
      &ArcServiceLauncher::OnArcAvailable, weak_factory_.GetWeakPtr()));
}

void ArcServiceLauncher::Shutdown() {
  DCHECK(arc_service_manager_);
  arc_service_manager_->arc_bridge_service()->Shutdown();
}

void ArcServiceLauncher::OnArcAvailable(bool arc_available) {
  arc_service_manager_->arc_bridge_service()->SetDetectedAvailability(
      arc_available);
}

}  // namespace arc
