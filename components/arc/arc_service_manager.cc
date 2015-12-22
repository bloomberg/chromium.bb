// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_service_manager.h"

#include "base/sequenced_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/arc/arc_bridge_bootstrap.h"
#include "components/arc/arc_bridge_service_impl.h"
#include "components/arc/input/arc_input_bridge.h"
#include "components/arc/power/arc_power_bridge.h"
#include "components/arc/settings/arc_settings_bridge.h"

namespace arc {

namespace {

// Weak pointer.  This class is owned by ChromeBrowserMainPartsChromeos.
ArcServiceManager* g_arc_service_manager = nullptr;

}  // namespace

ArcServiceManager::ArcServiceManager(
    scoped_ptr<ArcSettingsBridge> settings_bridge)
    : arc_bridge_service_(
          new ArcBridgeServiceImpl(ArcBridgeBootstrap::Create())),
      arc_settings_bridge_(std::move(settings_bridge)) {
  DCHECK(!g_arc_service_manager);
  arc_input_bridge_ = ArcInputBridge::Create(arc_bridge_service_.get());
  arc_power_bridge_.reset(new ArcPowerBridge(arc_bridge_service_.get()));
  g_arc_service_manager = this;

  arc_settings_bridge_->StartObservingBridgeServiceChanges();
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

}  // namespace arc
