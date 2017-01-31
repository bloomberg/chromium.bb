// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_service_manager.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task_runner.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_session.h"
#include "components/arc/arc_session_runner.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"

namespace arc {
namespace {

// Weak pointer.  This class is owned by arc::ArcServiceLauncher.
ArcServiceManager* g_arc_service_manager = nullptr;

}  // namespace

ArcServiceManager::ArcServiceManager(
    scoped_refptr<base::TaskRunner> blocking_task_runner)
    : blocking_task_runner_(blocking_task_runner),
      arc_bridge_service_(base::MakeUnique<ArcBridgeService>()),
      activity_resolver_(new LocalActivityResolver()) {
  DCHECK(!g_arc_service_manager);
  g_arc_service_manager = this;
}

ArcServiceManager::~ArcServiceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(g_arc_service_manager, this);
  g_arc_service_manager = nullptr;
}

// static
ArcServiceManager* ArcServiceManager::Get() {
  if (!g_arc_service_manager)
    return nullptr;
  DCHECK(g_arc_service_manager->thread_checker_.CalledOnValidThread());
  return g_arc_service_manager;
}

ArcBridgeService* ArcServiceManager::arc_bridge_service() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return arc_bridge_service_.get();
}

bool ArcServiceManager::AddServiceInternal(
    const std::string& name,
    std::unique_ptr<ArcService> service) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!name.empty() && services_.count(name) != 0) {
    LOG(ERROR) << "Ignoring registration of service with duplicate name: "
               << name;
    return false;
  }
  services_.insert(std::make_pair(name, std::move(service)));
  return true;
}

ArcService* ArcServiceManager::GetNamedServiceInternal(
    const std::string& name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (name.empty()) {
    LOG(ERROR) << "kArcServiceName[] should be a fully-qualified class name.";
    return nullptr;
  }
  auto service = services_.find(name);
  if (service == services_.end()) {
    LOG(ERROR) << "Named service " << name << " not found";
    return nullptr;
  }
  return service->second.get();
}

void ArcServiceManager::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  activity_resolver_ = nullptr;
  services_.clear();
}

}  // namespace arc
