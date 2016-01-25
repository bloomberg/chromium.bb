// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_service_manager.h"

#include "base/sequenced_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/arc/arc_bridge_bootstrap.h"
#include "components/arc/arc_bridge_service_impl.h"
#include "components/arc/clipboard/arc_clipboard_bridge.h"
#include "components/arc/ime/arc_ime_bridge.h"
#include "components/arc/input/arc_input_bridge.h"
#include "components/arc/power/arc_power_bridge.h"
#include "ui/arc/notification/arc_notification_manager.h"

namespace arc {

namespace {

// Weak pointer.  This class is owned by ChromeBrowserMainPartsChromeos.
ArcServiceManager* g_arc_service_manager = nullptr;

}  // namespace

ArcServiceManager::ArcServiceManager()
    : arc_bridge_service_(
          new ArcBridgeServiceImpl(ArcBridgeBootstrap::Create())) {
  DCHECK(!g_arc_service_manager);
  g_arc_service_manager = this;

  AddService(make_scoped_ptr(new ArcClipboardBridge(arc_bridge_service())));
  AddService(make_scoped_ptr(new ArcImeBridge(arc_bridge_service())));
  AddService(make_scoped_ptr(new ArcInputBridge(arc_bridge_service())));
  AddService(make_scoped_ptr(new ArcPowerBridge(arc_bridge_service())));
}

ArcServiceManager::~ArcServiceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(g_arc_service_manager == this);
  g_arc_service_manager = nullptr;
}

// static
ArcServiceManager* ArcServiceManager::Get() {
  DCHECK(g_arc_service_manager);
  DCHECK(g_arc_service_manager->thread_checker_.CalledOnValidThread());
  return g_arc_service_manager;
}

ArcBridgeService* ArcServiceManager::arc_bridge_service() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return arc_bridge_service_.get();
}

void ArcServiceManager::AddService(scoped_ptr<ArcService> service) {
  DCHECK(thread_checker_.CalledOnValidThread());

  services_.emplace_back(std::move(service));
}

void ArcServiceManager::OnPrimaryUserProfilePrepared(
    const AccountId& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  AddService(make_scoped_ptr(
      new ArcNotificationManager(arc_bridge_service(), account_id)));
}

}  // namespace arc
