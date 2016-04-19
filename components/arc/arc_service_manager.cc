// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_service_manager.h"

#include "base/sequenced_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/arc/arc_bridge_bootstrap.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_bridge_service_impl.h"
#include "components/arc/audio/arc_audio_bridge.h"
#include "components/arc/bluetooth/arc_bluetooth_bridge.h"
#include "components/arc/clipboard/arc_clipboard_bridge.h"
#include "components/arc/crash_collector/arc_crash_collector_bridge.h"
#include "components/arc/ime/arc_ime_service.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/metrics/arc_metrics_service.h"
#include "components/arc/net/arc_net_host_impl.h"
#include "components/arc/power/arc_power_bridge.h"
#include "ui/arc/notification/arc_notification_manager.h"

namespace arc {

namespace {

// Weak pointer.  This class is owned by ChromeBrowserMainPartsChromeos.
ArcServiceManager* g_arc_service_manager = nullptr;

// This pointer is owned by ArcServiceManager.
ArcBridgeService* g_arc_bridge_service_for_testing = nullptr;

}  // namespace

ArcServiceManager::ArcServiceManager() {
  DCHECK(!g_arc_service_manager);
  g_arc_service_manager = this;

  if (g_arc_bridge_service_for_testing) {
    arc_bridge_service_.reset(g_arc_bridge_service_for_testing);
    g_arc_bridge_service_for_testing = nullptr;
  } else {
    arc_bridge_service_.reset(new ArcBridgeServiceImpl(
        ArcBridgeBootstrap::Create()));
  }

  AddService(make_scoped_ptr(new ArcAudioBridge(arc_bridge_service())));
  AddService(make_scoped_ptr(new ArcBluetoothBridge(arc_bridge_service())));
  AddService(make_scoped_ptr(new ArcClipboardBridge(arc_bridge_service())));
  AddService(
      make_scoped_ptr(new ArcCrashCollectorBridge(arc_bridge_service())));
  AddService(make_scoped_ptr(new ArcImeService(arc_bridge_service())));
  AddService(make_scoped_ptr(new ArcIntentHelperBridge(arc_bridge_service())));
  AddService(make_scoped_ptr(new ArcMetricsService(arc_bridge_service())));
  AddService(make_scoped_ptr(new ArcNetHostImpl(arc_bridge_service())));
  AddService(make_scoped_ptr(new ArcPowerBridge(arc_bridge_service())));
}

ArcServiceManager::~ArcServiceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(g_arc_service_manager == this);
  g_arc_service_manager = nullptr;
  if (g_arc_bridge_service_for_testing) {
    delete g_arc_bridge_service_for_testing;
  }
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

void ArcServiceManager::Shutdown() {
  services_.clear();
}

//static
void ArcServiceManager::SetArcBridgeServiceForTesting(
    scoped_ptr<ArcBridgeService> arc_bridge_service) {
  if (g_arc_bridge_service_for_testing) {
    delete g_arc_bridge_service_for_testing;
  }
  g_arc_bridge_service_for_testing = arc_bridge_service.release();
}

}  // namespace arc
