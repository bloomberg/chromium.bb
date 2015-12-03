// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service.h"

#include "base/command_line.h"
#include "base/sequenced_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service_impl.h"

namespace arc {

namespace {

// Weak pointer.  This class is owned by ChromeBrowserMainPartsChromeos.
ArcBridgeService* g_arc_bridge_service = nullptr;

}  // namespace

ArcBridgeService::ArcBridgeService()
    : origin_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      available_(false),
      state_(State::STOPPED) {
  DCHECK(!g_arc_bridge_service);
  g_arc_bridge_service = this;
}

ArcBridgeService::~ArcBridgeService() {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  DCHECK(state() == State::STOPPING || state() == State::STOPPED);
  DCHECK(g_arc_bridge_service == this);
  g_arc_bridge_service = nullptr;
}

// static
ArcBridgeService* ArcBridgeService::Get() {
  DCHECK(g_arc_bridge_service);
  DCHECK(g_arc_bridge_service->origin_task_runner()->
      RunsTasksOnCurrentThread());
  return g_arc_bridge_service;
}

// static
bool ArcBridgeService::GetEnabled(const base::CommandLine* command_line) {
  return command_line->HasSwitch(chromeos::switches::kEnableArc);
}

void ArcBridgeService::AddObserver(Observer* observer) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  observer_list_.AddObserver(observer);
}

void ArcBridgeService::RemoveObserver(Observer* observer) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  observer_list_.RemoveObserver(observer);
}

void ArcBridgeService::AddAppObserver(AppObserver* observer) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  app_observer_list_.AddObserver(observer);
}

void ArcBridgeService::RemoveAppObserver(AppObserver* observer) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  app_observer_list_.RemoveObserver(observer);
}

void ArcBridgeService::SetState(State state) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  // DCHECK on enum classes not supported.
  DCHECK(state_ != state);
  state_ = state;
  FOR_EACH_OBSERVER(Observer, observer_list(), OnStateChanged(state_));
}

void ArcBridgeService::SetAvailable(bool available) {
  DCHECK(origin_task_runner()->RunsTasksOnCurrentThread());
  DCHECK(available_ != available);
  available_ = available;
  FOR_EACH_OBSERVER(Observer, observer_list(), OnAvailableChanged(available_));
}

// static
scoped_ptr<ArcBridgeService> ArcBridgeService::Create(
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& file_task_runner) {
  return make_scoped_ptr(new ArcBridgeServiceImpl(ipc_task_runner,
                                                  file_task_runner));
}

}  // namespace arc
