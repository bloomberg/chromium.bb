// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_service_manager.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_bridge_service_impl.h"

namespace arc {

namespace {

// Weak pointer.  This class is owned by arc::ArcServiceLauncher.
ArcServiceManager* g_arc_service_manager = nullptr;

// This pointer is owned by ArcServiceManager.
ArcBridgeService* g_arc_bridge_service_for_testing = nullptr;

}  // namespace

ArcServiceManager::ArcServiceManager(
    scoped_refptr<base::TaskRunner> blocking_task_runner)
    : blocking_task_runner_(blocking_task_runner),
      icon_loader_(new ActivityIconLoader()),
      activity_resolver_(new LocalActivityResolver()) {
  DCHECK(!g_arc_service_manager);
  g_arc_service_manager = this;

  if (g_arc_bridge_service_for_testing) {
    arc_bridge_service_.reset(g_arc_bridge_service_for_testing);
    g_arc_bridge_service_for_testing = nullptr;
  } else {
    arc_bridge_service_.reset(new ArcBridgeServiceImpl(blocking_task_runner));
  }
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

// static
bool ArcServiceManager::IsInitialized() {
  return g_arc_service_manager;
}

ArcBridgeService* ArcServiceManager::arc_bridge_service() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return arc_bridge_service_.get();
}

void ArcServiceManager::AddService(std::unique_ptr<ArcService> service) {
  DCHECK(thread_checker_.CalledOnValidThread());
  services_.emplace_back(std::move(service));
}

void ArcServiceManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcServiceManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ArcServiceManager::OnAppsUpdated() {
  for (auto& observer : observer_list_)
    observer.OnAppsUpdated();
}

void ArcServiceManager::Shutdown() {
  icon_loader_ = nullptr;
  activity_resolver_ = nullptr;
  services_.clear();
  arc_bridge_service_->OnShutdown();
}

// static
void ArcServiceManager::SetArcBridgeServiceForTesting(
    std::unique_ptr<ArcBridgeService> arc_bridge_service) {
  if (g_arc_bridge_service_for_testing)
    delete g_arc_bridge_service_for_testing;
  g_arc_bridge_service_for_testing = arc_bridge_service.release();
}

}  // namespace arc
